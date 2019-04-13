#ifndef __EM_H
#define __EM_H

#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>

#define EM_MULTICAST_GROUP "239.255.77.88"
#define EM_MULTICAST_PORT  4020
#define EM_MAX_UDP_SIZE 64

#define EM_MAX_CLIENTS 9
#define EM_MAX_DEVICES  9
#define EM_MAX_MAPPINGS 99

#define EM_MAX_STR 500
#define EM_INPUT_DEV_DIR "/sys/class/input"
#define EM_INPUT_DEV_PREFIX "event"

#define EM_MAX_COMBO 4
#define EM_MAX_OUTPUT_EVENTS 64

typedef struct em_client_type em_client;
typedef struct em_client_type {
    int                 idx;
    int                 combo[EM_MAX_COMBO];

    int                 local;
    char               *key;

    em_client          *next;
} em_client;

typedef struct em_mapping_type em_mapping;
typedef struct em_mapping_type {
    int                 combo[EM_MAX_COMBO];
    struct input_event *output[EM_MAX_OUTPUT_EVENTS];
    int                 filter_last;
    int                 release_pressed;
    int                 always_client;
    int                 only_device;

    int                 send_output;

    em_mapping         *next;
} em_mapping;

typedef struct em_device_type em_device;
typedef struct em_device_type {
    // Filled by jsmn_cfg_parse()
    int                     idx;
    uint16_t                product_id;
    uint16_t                vendor_id;
    char                   *name;
    char                   *check_capability;
    em_device              *next;

    // Active state
    int                     active;
    int                     pollfd_idx;

    // Filter KEY/BTN release
    uint16_t                filter_release_code;

    // Filled by em_grab_devices()
    char                   *device;
    int                     evfd;
    struct libevdev        *evdev;
    int                     uifd;
    struct libevdev_uinput *uidev;
} em_device;


#define EM_ENC_CLEAR_LEN 12
#define EM_ENC_ENC_LEN   16
#define EM_CLEAR_PACKET_LEN (EM_ENC_CLEAR_LEN + 2)
#define EM_ENC_PACKET_LEN   (EM_ENC_ENC_LEN   + 2)
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

void      em_usage();
void      em_fatal(const char* format, ...);
void*     em_malloc(int size);
int       em_event_code_from_name(const char *name);
const char * em_event_code_get_name(unsigned int code);

void      jsmn_cfg_parse(char *, em_device **, em_mapping **, em_client **em_client);

#endif
