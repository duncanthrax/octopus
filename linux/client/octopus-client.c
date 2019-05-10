#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <xxtea.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>

#define DEFAULT_MULTICAST_GROUP "239.255.77.88"
#define DEFAULT_PORT 4020

#define MIN_PACKET_SIZE 14
#define MAX_PACKET_SIZE 18

struct __attribute__((__packed__)) em_packet {
    // Sent unencrypted
    uint8_t                 clientIdx;  // 1
    uint8_t                 enc;        // 1
    // Encrypted parts, sending 12 bytes in the clear, 16 when encrypted.
    uint32_t                rnd;        // 4
    uint16_t                type;       // 2
    uint16_t                code;       // 2
     int32_t                value;      // 4
    // Extra bytes for encryption
    uint32_t                _space_;    // 4
} em_packet;

static void show_usage(const char *arg0)
{
  fprintf(stderr, "\n");
  fprintf(stderr, "Usage: %s [-c <clientID>] [-k <encKey>] [-p <port>] [-i <iface>] [-g <group>]\n", arg0);
  fprintf(stderr, "\n");
  fprintf(stderr, "         All command line options are optional. Default is to use clientID 1,\n");
  fprintf(stderr, "         no encryption, group address 239.255.77.77 and port 4020.\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "         -c <clientId>: Set client ID.\n");
  fprintf(stderr, "         -k <encKey>  : Enable encryption by setting encryption key.\n");
  fprintf(stderr, "         -i <iface>   : Use local interface <iface>. Either the IP\n");
  fprintf(stderr, "                        or the interface name can be specified.\n");
  // fprintf(stderr, "         -p <port>    : Use <port> instead of default port 4020.\n");
  // fprintf(stderr, "         -g <group>   : Multicast group address.\n");
  fprintf(stderr, "\n");
  exit(1);
}

static in_addr_t get_interface(const char *name)
{
  int sockfd = socket(AF_INET,SOCK_DGRAM,0);
  struct ifreq ifr;
  in_addr_t addr = inet_addr(name);
  struct if_nameindex *ni;
  int i;

  if (addr != INADDR_NONE) {
    return addr;
  }

  if (strlen(name) >= sizeof(ifr.ifr_name)) {
    fprintf(stderr, "Interface name too long: %s\n\n", name);
    goto error_exit;
  }
  strcpy(ifr.ifr_name, name);

  sockfd = socket(AF_INET,SOCK_DGRAM,0);
  if (ioctl(sockfd, SIOCGIFADDR, &ifr) != 0) {
    fprintf(stderr, "Invalid interface: %s\n\n", name);
    goto error_exit;
  }
  close(sockfd);
  return ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr;

error_exit:
  ni = if_nameindex();
  fprintf(stderr, "Available interfaces:\n");
  for (i = 0; ni[i].if_name != NULL; i++) {
    strcpy(ifr.ifr_name, ni[i].if_name);
    if (ioctl(sockfd, SIOCGIFADDR, &ifr) == 0) {
      fprintf(stderr, "  %-10s (%s)\n", ni[i].if_name, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
    }
  }
  exit(1);
}

int main(int argc, char*argv[]) {
  // Command line options
  int         client_id = 1;
  char *multicast_group = NULL;
  char  *encryption_key = NULL;
  in_addr_t   interface = INADDR_ANY;
  uint16_t         port = DEFAULT_PORT;

  int opt;
  while ((opt = getopt(argc, argv, "i:g:p:c:k:")) != -1) {
    switch (opt) {
    case 'i':
      interface = get_interface(optarg);
      break;
    case 'p':
      port = atoi(optarg);
      if (!port) show_usage(argv[0]);
      break;
    case 'c':
      client_id = atoi(optarg);
      break;
    case 'g':
      multicast_group = strdup(optarg);
      break;
    case 'k':
      encryption_key = strdup(optarg);
      break;
    default:
      show_usage(argv[0]);
    }
  }
  if (optind < argc) {
    fprintf(stderr, "Expected argument after options\n");
    show_usage(argv[0]);
  }

  printf("Client idx #%u, encryption %s\n", client_id, encryption_key ? "enabled" : "disabled");

  struct libevdev_uinput *uiodev;
  struct libevdev *odev = libevdev_new();
  libevdev_set_name(odev, "Octopus Output");

  libevdev_enable_event_type(odev, EV_SYN);
  for (int k = 0; k < SYN_CNT; k++)
    if (libevdev_event_code_get_name(EV_SYN, k)) libevdev_enable_event_code(odev, EV_SYN, k, NULL);

  libevdev_enable_event_type(odev, EV_KEY);
  for (int k = 0; k < KEY_CNT; k++)
    if (libevdev_event_code_get_name(EV_KEY, k)) libevdev_enable_event_code(odev, EV_KEY, k, NULL);

  libevdev_enable_event_type(odev, EV_REL);
  for (int k = 0; k < REL_CNT; k++)
    if (libevdev_event_code_get_name(EV_REL, k)) libevdev_enable_event_code(odev, EV_REL, k, NULL);

  if (libevdev_uinput_create_from_device(odev, LIBEVDEV_UINPUT_OPEN_MANAGED, &uiodev) != 0) {
    printf("Unable to open output device.\n");
    exit(-1);
  }

  int sockfd = socket(AF_INET,SOCK_DGRAM,0);

  struct sockaddr_in servaddr;
  memset((void *)&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(port);
  bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

  struct ip_mreq imreq;
  memset(&imreq, 0, sizeof(imreq));
  imreq.imr_multiaddr.s_addr = inet_addr(multicast_group ? multicast_group : DEFAULT_MULTICAST_GROUP);
  imreq.imr_interface.s_addr = interface;

  setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
            (const void *)&imreq, sizeof(struct ip_mreq));

  printf("Listening for events at %s:%u\n",
    multicast_group ? multicast_group : DEFAULT_MULTICAST_GROUP,
    port);

  for (;;) {
    struct em_packet packet;
    size_t n = recvfrom(sockfd, &packet, MAX_PACKET_SIZE, 0, NULL, 0);
    if (n < MIN_PACKET_SIZE) continue;
    if (packet.clientIdx != client_id) continue;

    if (packet.enc) {
      unsigned char *decrypt_data = xxtea_decrypt(&(packet.rnd), packet.enc, encryption_key, &n);
      if (!decrypt_data || n > MAX_PACKET_SIZE - 2) continue;
      memcpy(&(packet.rnd), decrypt_data, n);
      free(decrypt_data);
    }

    int rc = libevdev_uinput_write_event(uiodev, packet.type, packet.code, packet.value);
    if (rc != 0) {
      printf("Sending event failed with rc %d on uinput device.\n", rc);
      exit(-1);
    }
  }

  BAIL:
  return 0;
};

