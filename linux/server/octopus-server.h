#ifndef __EM_H
#define __EM_H

#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>

#define EM_MULTICAST_GROUP "239.255.77.88"
#define EM_MULTICAST_PORT  4020

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

    em_client          *next;
} em_client;

typedef struct em_mapping_type em_mapping;
typedef struct em_mapping_type {
    int                 combo[EM_MAX_COMBO];
    struct input_event *output[EM_MAX_OUTPUT_EVENTS];
    int                 filter_last;
    int                 release_pressed;

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

    // Filled by em_grab_devices()
    char                   *device;
    int                     evfd;
    struct libevdev        *evdev;
    int                     uifd;
    struct libevdev_uinput *uidev;
} em_device;

struct __attribute__((__packed__)) em_packet {
    uint8_t                 clientIdx;
    uint16_t                type;
    uint16_t                code;
    int32_t                 value;
} em_packet;

void      em_usage();
void      em_fatal(const char* format, ...);
void*     em_malloc(int size);
int       em_event_code_from_name(const char *name);
const char * em_event_code_get_name(unsigned int code);

void      jsmn_cfg_parse(char *, em_device **, em_mapping **, em_client **em_client);

#endif
