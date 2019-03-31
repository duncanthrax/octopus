#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <poll.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>

#include "octopus-server.h"

void em_usage() {
    printf("Usage!\n");
    exit(-1);
}

void em_fatal(const char* format, ...) {
    va_list arglist;

    printf("Fatal: ");
    va_start(arglist, format);
    vprintf(format, arglist);
    va_end(arglist);
    printf("\n");
    exit(-1);
}

void* em_malloc(int size) {
    void *ok = malloc(size);
    if (!ok) em_fatal("malloc failed for %d bytes\n", size);
    memset(ok, 0, size);
    return ok;
}


int em_event_code_from_name(const char *name) {
    if (strcmp(name, "WHEEL_UP")    == 0) return 0x400;
    if (strcmp(name, "WHEEL_RIGHT") == 0) return 0x401;
    if (strcmp(name, "WHEEL_DOWN")  == 0) return 0x402;
    if (strcmp(name, "WHEEL_LEFT")  == 0) return 0x403;
    return libevdev_event_code_from_name(EV_KEY, name);
}

const char * em_event_code_get_name(unsigned int code) {
    if (code == 0x400) return "WHEEL_UP";
    if (code == 0x401) return "WHEEL_RIGHT";
    if (code == 0x402) return "WHEEL_DOWN";
    if (code == 0x403) return "WHEEL_LEFT";
    return libevdev_event_code_get_name(EV_KEY, code);
}

void em_grab_devices(em_device *devices) {
    struct dirent **event_dev_list;

    #define EM_MAX_CMP_CHARS 4
    int cmp_file(char *fpath, char *string) {
        char data[EM_MAX_CMP_CHARS];
        int fd = open(fpath, O_RDONLY);
        if (!fd) return 0;
        int i = read(fd, data, EM_MAX_CMP_CHARS);
        close(fd);
        if (i < strlen(string)) return 0;
        return strncmp(string, data, strlen(string)) == 0 ? 1 : 0;
    }

    int prefix_filter(const struct dirent *entry) {
        if (strncmp(entry->d_name, EM_INPUT_DEV_PREFIX, strlen(EM_INPUT_DEV_PREFIX)) == 0)
            return 1;
        return 0;
    }

    int num_entries = scandir(EM_INPUT_DEV_DIR, &event_dev_list, prefix_filter, NULL);
    if (num_entries < 0) {
        printf("Error: scandir(%s) error, errno %d\n", EM_INPUT_DEV_DIR, errno);
        return;
    }

    em_device* dev = devices;
    while (dev) {
        char fpath[EM_MAX_STR+1];
        char cmpstr[EM_MAX_STR+1];
        if (dev->active) goto NEXT_DEV;
        int found = 0;
        for (int i = 0; i < num_entries; i++) {
            snprintf(fpath, EM_MAX_STR, EM_INPUT_DEV_DIR"/%s/device/id/product", event_dev_list[i]->d_name);
            snprintf(cmpstr, EM_MAX_STR, "%04hx", dev->product_id);
            if (cmp_file(fpath, cmpstr)) {
                snprintf(fpath, EM_MAX_STR, EM_INPUT_DEV_DIR"/%s/device/id/vendor", event_dev_list[i]->d_name);
                snprintf(cmpstr, EM_MAX_STR, "%04hx", dev->vendor_id);
                if (cmp_file(fpath, cmpstr)) {
                    int ok = 1;

                    if (dev->name) {
                        snprintf(fpath, EM_MAX_STR, EM_INPUT_DEV_DIR"/%s/device/name", event_dev_list[i]->d_name);
                        int fd = open(fpath, O_RDONLY);
                        if (fd >= 0) {
                            int rc = read(fd, cmpstr, EM_MAX_STR);
                            if (rc > 0) {
                                cmpstr[rc] = '\0';
                                char *lf = strchr(cmpstr, '\n');
                                if (lf) *lf = '\0';
                                if (strcmp(cmpstr, dev->name) != 0) ok = 0;
                            }
                            else ok = 0;

                            close(fd);
                        }
                        else ok = 0;
                    }

                    if (dev->check_capability) {
                        snprintf(fpath, EM_MAX_STR, EM_INPUT_DEV_DIR"/%s/device/capabilities/%s", event_dev_list[i]->d_name, dev->check_capability);
                        if (cmp_file(fpath, "0")) ok = 0;
                    }

                    if (ok) {
                        if (found) {
                            if (!dev->device) printf("Device #%d: Found more than one device, only using %s\n", dev->idx, dev->device);
                        }
                        else {
                            if (dev->device) free(dev->device);
                            found = 1;
                            dev->device = strdup(event_dev_list[i]->d_name);
                            if (!dev->device) em_fatal("strdup() failed");
                        }
                    }
                }
            }
        }

        if (found) {
            snprintf(fpath, EM_MAX_STR, "/dev/input/%s", dev->device);
            dev->evfd = open(fpath, O_RDONLY|O_NONBLOCK);
            if (dev->evfd < 0 || libevdev_new_from_fd(dev->evfd, &(dev->evdev)) < 0) {
                printf("Device #%d: Unable to open %s\n", dev->idx, fpath);
                goto DEACTIVATE_DEV;
            }

            if (libevdev_grab(dev->evdev, LIBEVDEV_GRAB) < 0) {
                printf("Device #%d: Unable to grab device %s\n", dev->idx, fpath);
                goto DEACTIVATE_DEV;
            }

            char *name = strdup(libevdev_get_name(dev->evdev));
            if (!name) em_fatal("strdup() failed");
            snprintf(fpath, EM_MAX_STR, "%s [evdev-mapper]", name);
            libevdev_set_name(dev->evdev, fpath);
            free(name);

            dev->uifd = open("/dev/uinput", O_RDWR);
            if (dev->uifd < 0 || libevdev_uinput_create_from_device(dev->evdev, dev->uifd, &(dev->uidev)) != 0) {
                printf("Device #%d: Unable to open uinput device\n", dev->idx);
                goto DEACTIVATE_DEV;
            }

            dev->active = 1;
            printf("Device #%d: Using device node %s\n", dev->idx, dev->device);

            goto NEXT_DEV;
        }

        DEACTIVATE_DEV:

        if (dev->evfd) {
            close(dev->evfd);
            dev->evfd = 0;
        }
        if (dev->evdev) {
            libevdev_free(dev->evdev);
            dev->evdev = NULL;
        }
        if (dev->uifd) {
            close(dev->uifd);
            dev->uifd = 0;
        }
        if (dev->uidev) {
            libevdev_uinput_destroy(dev->uidev);
            dev->uidev = NULL;
        }

        dev->active = 0;

        if (dev->device) {
            printf("Device #%d: Device inactive\n", dev->idx);
            free(dev->device);
            dev->device = NULL;
        }

        NEXT_DEV:
        dev = dev->next;
    }

    for (int i = 0; i < num_entries; i++) {
        free(event_dev_list[i]);
    }
    free(event_dev_list);
}

int check_combo_array(int *array, int value) {
    for (int k = 0; k < EM_MAX_COMBO; k++) {
        if (array[k] == value) return 1;
    }
    return 0;
}

int main(int argc, char* argv[]) {

    setvbuf(stdout, NULL, _IONBF, 0);

    if (argc < 2) em_usage();

    // Read config file passed on cmdline.
    em_device  *devices;
    em_mapping *mappings;
    em_client  *clients;
    jsmn_cfg_parse(argv[1], &devices, &mappings, &clients); // Will quit on errors.
    if (!devices)  em_fatal("No input devices specified in configuration.");
    if (!mappings) em_fatal("No mappings specified in configuration.");
    if (!clients)  em_fatal("No clients specified in configuration.");

    // Open our output device. Enable for all KEY_* and BTN_*.
    struct libevdev_uinput *uiodev;
    struct libevdev *odev = libevdev_new();
    libevdev_set_name(odev, "evdev-mapper output");
    libevdev_enable_event_type(odev, EV_KEY);
    for (int k = 0; k < KEY_CNT; k++) {
        if (em_event_code_get_name(k)) libevdev_enable_event_code(odev, EV_KEY, k, NULL);
    }
    if (libevdev_uinput_create_from_device(odev, LIBEVDEV_UINPUT_OPEN_MANAGED, &uiodev) != 0)
        em_fatal("Unable to open output device.");

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) em_fatal("Unable to open socket.");
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(EM_MULTICAST_GROUP);
    addr.sin_port = htons(EM_MULTICAST_PORT);

    // Array holding currently pressed keys.
    int active_keys[EM_MAX_COMBO];
    memset(active_keys, 0, sizeof(active_keys));

    // Currently active client
    em_client *active_client = clients;

    void send_remote_event(uint16_t type, uint16_t code, int32_t value) {
        struct em_packet packet;
        packet.clientIdx = (uint8_t) active_client->idx;
        packet.type      = type;
        packet.code      = code;
        packet.value     = value;
        if (sendto(sock, &packet, sizeof(em_packet), 0, (struct sockaddr *)&addr, sizeof(addr)) < 0)
            em_fatal("Sending UDP packet failed.");
    }

    void send_event(em_device *dev, uint16_t type, uint16_t code, int32_t value) {
        if (active_client->local) {
            if (dev) {
                int rc = libevdev_uinput_write_event(dev->uidev, type, code, value);
                if (rc != 0) {
                    printf("Device #%d: Sending event failed with rc %d, deactivating.\n", dev->idx, rc);
                    dev->active = 0;
                }
            }
            else {
                int rc = libevdev_uinput_write_event(uiodev, type, code, value);
                if (rc != 0) em_fatal("Sending event failed with rc %d on uinput device.", rc);
            }
        }
        else send_remote_event(type, code, value);
    }

    void release_pressed() {
        // Send release events for pressed keys on all devices
        if (active_client->local) {
            em_device *dev = devices;
            while (dev) {
                for (int k = 0; k < EM_MAX_COMBO; k++) {
                    if (active_keys[k])
                        libevdev_uinput_write_event(dev->uidev, EV_KEY, active_keys[k], 0);
                }
                libevdev_uinput_write_event(dev->uidev, EV_SYN, SYN_REPORT, 0);
                dev = dev->next;
            }
        }
        else {
            for (int k = 0; k < EM_MAX_COMBO; k++) {
                if (active_keys[k])
                    send_remote_event(EV_KEY, active_keys[k], 0);
            }
        }
        for (int k = 0; k < EM_MAX_COMBO; k++) { active_keys[k] = 0; };
    }

    // Main loop
    while (1) {

        // Check if new devices have shown up
        em_grab_devices(devices);

        // Set up pollfds
        struct pollfd pollfds[EM_MAX_DEVICES];
        memset(pollfds, 0, sizeof(pollfds));
        int num_devs = 0;
        em_device * dev = devices;
        while (dev) {
            if (dev->active) {
                pollfds[dev->idx].fd = dev->evfd;
                pollfds[dev->idx].events = POLLIN;
                num_devs++;
            }
            dev = dev->next;
        }

        time_t last_device_check = time(NULL);
        em_client *switch_client = NULL;

        // poll() loop, quit for device rescan every five seconds
        while (1) {
            int rc = poll(pollfds, num_devs, 1000);
            // All but EINTR are deadly
            if (rc < 0 && errno != EINTR) em_fatal("poll() failed with errno %d\n", errno);
            // poll() timeout
            if (rc == 0) goto NEXT_POLL;

            dev = devices;
            while (dev) {
                // Skip nonexistant devices
                if (!dev->active)
                    goto NEXT_DEV;

                // Check for errors
                if ( (pollfds[dev->idx].revents & POLLERR) ||
                     (pollfds[dev->idx].revents & POLLHUP) ||
                     (pollfds[dev->idx].revents & POLLNVAL) ) {
                    printf("Device #%d: poll() error on fd (revents 0x%04hx), deactivating.\n", dev->idx, pollfds[dev->idx].revents);
                    dev->active = 0;
                    goto NEXT_DEV;
                }

                // Check for readable events
                if (!(pollfds[dev->idx].revents & POLLIN))
                    goto NEXT_DEV;

                int mode = LIBEVDEV_READ_FLAG_NORMAL;
                while (1) {
                    struct input_event ie;
                    rc = libevdev_next_event(dev->evdev, mode, &ie);
                    if (rc < 0) break;
                    if (rc == LIBEVDEV_READ_STATUS_SYNC && mode == LIBEVDEV_READ_FLAG_NORMAL) {
                        // Resynch device
                        printf("Device #%d: resyncing\n", dev->idx);
                        mode = LIBEVDEV_READ_STATUS_SYNC;
                        continue;
                    }

                    // Keystate evaluation, set up active_keys[]

                    // printf("t:%s c:%s v:%d\n",
                    //     libevdev_event_type_get_name(ie.type),
                    //     libevdev_event_code_get_name(ie.type, ie.code),
                    //     ie.value
                    // );

                    int check_combos = 0;
                    int filter = 0;

                    if (ie.type == EV_REL && ie.code == REL_WHEEL) {
                        if (ie.value > 0) { ie.type = EV_KEY; ie.code = 0x400; ie.value = 1; };
                        if (ie.value < 0) { ie.type = EV_KEY; ie.code = 0x402; ie.value = 1; };
                    }
                    if (ie.type == EV_REL && ie.code == REL_HWHEEL) {
                        if (ie.value > 0) { ie.type = EV_KEY; ie.code = 0x401; ie.value = 1; };
                        if (ie.value < 0) { ie.type = EV_KEY; ie.code = 0x403; ie.value = 1; };
                    }

                    if (ie.type == EV_KEY && ie.value < 2) {
                        if (ie.value) {
                            // Key pressed
                            for (int k = 0; k < EM_MAX_COMBO; k++) {
                                if (!active_keys[k] || active_keys[k] == ie.code) {
                                    active_keys[k] = ie.code;
                                    check_combos = 1;
                                    break;
                                };
                            }
                        }
                        else {
                            // Key released
                            for (int k = 0; k < EM_MAX_COMBO; k++) {
                                if (active_keys[k] == ie.code) active_keys[k] = 0;
                            }
                        }
                    }

                    // printf("\nActive keys: ");
                    // for (int k = 0; k < EM_MAX_COMBO; k++) {
                    //     if (active_keys[k])
                    //         printf("%s ", em_event_code_get_name(active_keys[k]));
                    // }

                    if (check_combos) {

                        // Check mapping combos
                        em_mapping *mapping = mappings;
                        while (mapping) {
                            for (int k = 0; k < EM_MAX_COMBO; k++) {
                                if (active_keys[k] && !check_combo_array(mapping->combo, active_keys[k]))
                                    goto NEXT_MAPPING;
                            }
                            for (int k = 0; k < EM_MAX_COMBO; k++) {
                                if (mapping->combo[k] && !check_combo_array(active_keys, mapping->combo[k]))
                                    goto NEXT_MAPPING;
                            }

                            // Mark to send output event sequence
                            mapping->send_output = 1;
                            if (mapping->filter_last) filter = 1;

                            NEXT_MAPPING:
                            mapping = mapping->next;
                        }

                        // Check client combos
                        em_client *client = clients;
                        while (client) {

                            for (int k = 0; k < EM_MAX_COMBO; k++) {
                                if (active_keys[k] && !check_combo_array(client->combo, active_keys[k]))
                                    goto NEXT_CLIENT;
                            }
                            for (int k = 0; k < EM_MAX_COMBO; k++) {
                                if (client->combo[k] && !check_combo_array(active_keys, client->combo[k]))
                                    goto NEXT_CLIENT;
                            }

                            // Switch clients
                            switch_client = client;
                            filter = 1; // Always filter switch combos

                            NEXT_CLIENT:
                            client = client->next;
                        }
                    }

                    // Forward event to output if we don't want it filtered
                    if (!filter) {
                        send_event(dev, ie.type, ie.code, ie.value);
                    }
                }

                NEXT_DEV:
                dev = dev->next;
            }

            // Remove fake keys from active_keys, they have no release event.
            for (int k = 0; k < EM_MAX_COMBO; k++) {
                if (active_keys[k] >= 0x400) active_keys[k] = 0;
            }

            // Send pending output sequences
            em_mapping *mapping = mappings;
            while (mapping) {
                if (mapping->send_output) {
                    if (mapping->release_pressed) release_pressed();
                    for (int k = 0; k < EM_MAX_OUTPUT_EVENTS; k++) {
                        if (!mapping->output[k]) break;
                        if (mapping->output[k]->code >= 0x400) {
                            switch (mapping->output[k]->code) {
                                case 0x400:
                                    send_event(NULL, EV_REL, REL_WHEEL, 1);
                                break;
                                case 0x401:
                                    send_event(NULL, EV_REL, REL_HWHEEL, 1);
                                break;
                                case 0x402:
                                    send_event(NULL, EV_REL, REL_WHEEL, -1);
                                break;
                                case 0x403:
                                    send_event(NULL, EV_REL, REL_HWHEEL, -1);
                                break;
                            }
                        }
                        else send_event(NULL, mapping->output[k]->type, mapping->output[k]->code, mapping->output[k]->value);
                    }
                    send_event(NULL, EV_SYN, SYN_REPORT, 0);
                    mapping->send_output = 0;
                }

                mapping = mapping->next;
            }

            // Switch clients, if requested
            if (switch_client) {
                if (switch_client != active_client) {
                    release_pressed();
                    printf("Switching to client #%u\n", switch_client->idx);
                    active_client = switch_client;
                }
                switch_client = NULL;
            }

            // Check for device availability every ~3-4 seconds
            NEXT_POLL:
            if (last_device_check < (time(NULL) - 3)) break;
        }
    }
}
