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
#include <sys/types.h>
#include <sys/stat.h>
#include <jsmn/jsmn.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>

#include "octopus-server.h"

#define JSMN_NUM_TOKENS 128
#define JSMN_CFG_SIZE 16384

char jsmn_cfg[JSMN_CFG_SIZE];

char *jsmn_tmp_value_p = NULL;
char* jsmn_tmp_value(jsmntok_t token) {
    if (jsmn_tmp_value_p) free(jsmn_tmp_value_p);
    char *jsmn_tmp_value_p = em_malloc(token.end - token.start + 1);
    memcpy(jsmn_tmp_value_p, &jsmn_cfg[token.start], token.end - token.start);
    jsmn_tmp_value_p[token.end - token.start] = '\0';
    return jsmn_tmp_value_p;
}

int jsmn_cmp_value(char *str, jsmntok_t token) {
    return (strcmp(str, jsmn_tmp_value(token)) == 0) ? 1 : 0;
}

char* jsmn_get_value(jsmntok_t token) {
    char *str = em_malloc(token.end - token.start + 1);
    memcpy(str, &jsmn_cfg[token.start], token.end - token.start);
    str[token.end - token.start] = '\0';
    return str;
}

int jsmn_get_bool(jsmntok_t token) {
    char *str = em_malloc(token.end - token.start + 1);
    memcpy(str, &jsmn_cfg[token.start], token.end - token.start);
    str[token.end - token.start] = '\0';
    return (strcmp(str,"true") == 0) ? 1:0;
}

int jsmn_get_int(jsmntok_t token) {
    char *str = em_malloc(token.end - token.start + 1);
    memcpy(str, &jsmn_cfg[token.start], token.end - token.start);
    str[token.end - token.start] = '\0';
    return atoi(str);
}

int jsmn_skip(jsmntok_t *tokens, int t) {
    int i = 0;
    t++;
    if (t >= JSMN_NUM_TOKENS) return -1;
    if (tokens[t-1].type == JSMN_OBJECT) i = tokens[t-1].size * 2;
    if (tokens[t-1].type == JSMN_ARRAY)  i = tokens[t-1].size;
    while (i) {
        t = jsmn_skip(tokens, t);
        if (t < 0) return -1;
        i--;
    }
    return t < JSMN_NUM_TOKENS ? t : -1;
}

int jsmn_object_key_value(jsmntok_t *tokens, int t, char *key, jsmntype_t vtype) {
    if (tokens[t].type != JSMN_OBJECT) return -1;
    t++;
    while (t < JSMN_NUM_TOKENS) {
        switch (tokens[t].type) {
            case JSMN_STRING:
            case JSMN_PRIMITIVE:
                if (jsmn_cmp_value(key, tokens[t])
                    && t+1 < JSMN_NUM_TOKENS
                    && tokens[t+1].type == vtype) return t+1;
            case JSMN_OBJECT:
            case JSMN_ARRAY:
                t = jsmn_skip(tokens, t);
                if (t < 0) return -1;
            break;
            default: return -1;
        }
    }
    return t < JSMN_NUM_TOKENS ? t : -1;
}

int jsmn_array_index(jsmntok_t *tokens, int t, int index, jsmntype_t vtype) {
    if (tokens[t].type != JSMN_ARRAY) return -1;
    if (index > tokens[t].size) return -1;
    t++;
    if (t >= JSMN_NUM_TOKENS) return -1;
    while (index) {
        t = jsmn_skip(tokens, t);
        if (t < 0) return -1;
        index--;
    }
    if (t < JSMN_NUM_TOKENS && tokens[t].type == vtype) return t;
    return -1;
}

void jsmn_cfg_parse(char *fname, em_device **devices_p, em_mapping **mappings_p, em_client **clients_p) {

    int fd = open(fname, O_RDONLY);
    if (fd < 0) em_fatal("Unable to open config file");
    int rc = read(fd, jsmn_cfg, JSMN_CFG_SIZE);
    close(fd);
    if (rc <= 0) em_fatal("Unable parse config file");
    jsmn_cfg[rc] = '\0';

    jsmntok_t tokens[JSMN_NUM_TOKENS];
    memset(tokens, 0, sizeof(tokens));

    jsmn_parser parser;
    jsmn_init(&parser);
    if (jsmn_parse(&parser, jsmn_cfg, strlen(jsmn_cfg), tokens, JSMN_NUM_TOKENS) < 0 || tokens[0].type != JSMN_OBJECT)
        em_fatal("Config: Unable to parse config file.");

    int devices_tnum = jsmn_object_key_value(tokens, 0, "devices", JSMN_ARRAY);
    if (devices_tnum < 0)
        em_fatal("Config: 'devices' section not found or not an array.");

    *devices_p = NULL;
    em_device *dev = NULL;
    for (int dev_num = 0; (dev_num < tokens[devices_tnum].size && dev_num < EM_MAX_DEVICES); dev_num++) {
        em_device *d = em_malloc(sizeof(em_device));
        if (dev) dev->next = d;
        dev = d;
        if (!(*devices_p)) *devices_p = d;

        int device_tnum = jsmn_array_index(tokens, devices_tnum, dev_num, JSMN_OBJECT);
        if (device_tnum < 0)
            em_fatal("Config: 'devices' array must contain device objects.");

        dev->idx = dev_num;

        int scalar_tnum = jsmn_object_key_value(tokens, device_tnum, "product_id", JSMN_STRING);
        if (scalar_tnum < 0) em_fatal("Config: 'product_id' is mandatory");
        dev->product_id = (uint16_t)strtol(jsmn_tmp_value(tokens[scalar_tnum]), NULL, 0);

        scalar_tnum = jsmn_object_key_value(tokens, device_tnum, "vendor_id", JSMN_STRING);
        if (scalar_tnum < 0) em_fatal("Config: 'vendor_id' is mandatory");
        dev->vendor_id = (uint16_t)strtol(jsmn_tmp_value(tokens[scalar_tnum]), NULL, 0);

        scalar_tnum = jsmn_object_key_value(tokens, device_tnum, "name", JSMN_STRING);
        if (scalar_tnum > 0) dev->name = jsmn_get_value(tokens[scalar_tnum]);

        scalar_tnum = jsmn_object_key_value(tokens, device_tnum, "check_capability", JSMN_STRING);
        if (scalar_tnum > 0) dev->check_capability = jsmn_get_value(tokens[scalar_tnum]);

        printf("Device #%d: %04hx:%04hx ", dev->idx, dev->vendor_id, dev->product_id);
        if (dev->check_capability) printf("[%s] ", dev->check_capability);
        if (dev->name) printf("%s", dev->name);
        printf("\n");
    }

    int mappings_tnum = jsmn_object_key_value(tokens, 0, "mappings", JSMN_ARRAY);
    if (mappings_tnum < 0)
        em_fatal("Config: 'mappings' section not found or not an array.");

    *mappings_p = NULL;
    em_mapping *mapping = NULL;
    for (int mapping_num = 0; (mapping_num < tokens[mappings_tnum].size && mapping_num < EM_MAX_MAPPINGS); mapping_num++) {
        em_mapping *m = em_malloc(sizeof(em_mapping));
        if (mapping) mapping->next = m;
        mapping = m;
        if (!(*mappings_p)) *mappings_p = m;

        int mapping_tnum = jsmn_array_index(tokens, mappings_tnum, mapping_num, JSMN_OBJECT);
        if (mapping_tnum < 0)
            em_fatal("Config: 'mappings' array must contain mapping objects.");

        int scalar_tnum = jsmn_object_key_value(tokens, mapping_tnum, "filter_last", JSMN_PRIMITIVE);
        if (scalar_tnum > 0) mapping->filter_last = jsmn_get_bool(tokens[scalar_tnum]);

        scalar_tnum = jsmn_object_key_value(tokens, mapping_tnum, "release_pressed", JSMN_PRIMITIVE);
        if (scalar_tnum > 0) mapping->release_pressed = jsmn_get_bool(tokens[scalar_tnum]);

        scalar_tnum = jsmn_object_key_value(tokens, mapping_tnum, "always_client", JSMN_PRIMITIVE);
        if (scalar_tnum > 0)
            mapping->always_client = jsmn_get_int(tokens[scalar_tnum]);

        scalar_tnum = jsmn_object_key_value(tokens, mapping_tnum, "only_device", JSMN_PRIMITIVE);
        if (scalar_tnum > 0)
            mapping->only_device = jsmn_get_int(tokens[scalar_tnum]);

        int combo_tnum = jsmn_object_key_value(tokens, mapping_tnum, "combo", JSMN_ARRAY);
        if (combo_tnum < 0) em_fatal("Config: 'combo' is mandatory.");
        for (int event_num = 0; (event_num < tokens[combo_tnum].size && event_num < EM_MAX_COMBO); event_num++) {
            int event_tnum = combo_tnum + event_num + 1;
            if (tokens[event_tnum].type != JSMN_STRING)
                em_fatal("Config: event specifiers must be given as quoted strings.");
            char *tmpval = jsmn_tmp_value(tokens[event_tnum]);
            int code = em_event_code_from_name(tmpval);
            if (code < 0) em_fatal("Config: unknown key code '%s'.", tmpval);
            mapping->combo[event_num] = code;
        }

        int output_tnum = jsmn_object_key_value(tokens, mapping_tnum, "output", JSMN_ARRAY);
        if (output_tnum >= 0) {
            for (int event_num = 0; (event_num < tokens[output_tnum].size && event_num < EM_MAX_OUTPUT_EVENTS); event_num++) {
                int event_tnum = output_tnum + event_num + 1;
                if (tokens[event_tnum].type != JSMN_STRING)
                    em_fatal("Config: event specifiers must be gives as quoted strings.");
                mapping->output[event_num] = em_malloc(sizeof(struct input_event));
                char *tmpval = jsmn_tmp_value(tokens[event_tnum]);
                if (strlen(tmpval) < 5) em_fatal("Config: Invalid event specifier.");
                mapping->output[event_num]->value = (tmpval[0] == '-') ? 0 : 1;
                tmpval = &tmpval[1];
                mapping->output[event_num]->type = EV_KEY;
                int code = em_event_code_from_name(tmpval);
                if (code < 0) em_fatal("Config: unknown key code in event.");
                mapping->output[event_num]->code = code;
            }
        }
    }

    *clients_p = NULL;
    int clients_tnum = jsmn_object_key_value(tokens, 0, "clients", JSMN_ARRAY);
    if (clients_tnum < 0) return;

    em_client *client = NULL;
    for (int client_num = 0; (client_num < tokens[clients_tnum].size && client_num < EM_MAX_MAPPINGS); client_num++) {
        em_client *m = em_malloc(sizeof(em_client));
        if (client) client->next = m;
        client = m;
        if (!(*clients_p)) *clients_p = m;

        int client_tnum = jsmn_array_index(tokens, clients_tnum, client_num, JSMN_OBJECT);
        if (client_tnum < 0)
            em_fatal("Config: 'clients' array must contain client objects.");

        client->idx = client_num;

        int scalar_tnum = jsmn_object_key_value(tokens, client_tnum, "local", JSMN_PRIMITIVE);
        if (scalar_tnum > 0) client->local = jsmn_get_bool(tokens[scalar_tnum]);

        int combo_tnum = jsmn_object_key_value(tokens, client_tnum, "combo", JSMN_ARRAY);
        if (combo_tnum > 0)
            for (int event_num = 0; (event_num < tokens[combo_tnum].size && event_num < EM_MAX_COMBO); event_num++) {
                int event_tnum = combo_tnum + event_num + 1;
                if (tokens[event_tnum].type != JSMN_STRING)
                    em_fatal("Config: event specifiers must be given as quoted strings.");
                char *tmpval = jsmn_tmp_value(tokens[event_tnum]);
                int code = em_event_code_from_name(tmpval);
                if (code < 0) em_fatal("Config: unknown key code '%s'.", tmpval);
                client->combo[event_num] = code;
            }
    }

}
