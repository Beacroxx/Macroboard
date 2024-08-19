#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <libudev.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <alsa/asoundlib.h>
#include "hid.h"
#include <ctype.h>

#define MAX_LINE 256
#define MAX_COMMANDS 10

// Trim whitespace from a string
void trim(char *str) {
	char *start = str;
	char *end = str + strlen(str) - 1;

	while(isspace((unsigned char)*start)) start++;
	while(end > start && isspace((unsigned char)*end)) end--;

	*(end + 1) = '\0';
	memmove(str, start, end - start + 2);
}

// Load a section of commands from a file
void load_config(const char* filename, char commands[MAX_COMMANDS][MAX_LINE], const char* section) {
	FILE* file = fopen(filename, "r");
	if (file == NULL) {
		printf("Error opening file\n");
		return;
	}

	char line[MAX_LINE];
	int in_section = 0;
	int command_index = 0;

	while (fgets(line, sizeof(line), file)) {
		trim(line);

		if (line[0] == '[') {
			char section_name[MAX_LINE];
			sscanf(line, "[%[^]]", section_name);
			in_section = (strcmp(section_name, section) == 0);
			continue;
		}

		if (in_section && command_index < MAX_COMMANDS) {
			char *equals = strchr(line, '=');
			if (equals) {
				char *value = equals + 1;
				trim(value);
				strncpy(commands[command_index], value, MAX_LINE - 1);
				commands[command_index][MAX_LINE - 1] = '\0';
				command_index++;
			}
		}
	}

	fclose(file);
}

// Set the volume of a mixer element
int set_volume(const char* element_name, long volume) {
	snd_mixer_t *handle;
	snd_mixer_elem_t *elem;
	snd_mixer_selem_id_t *sid;

	snd_mixer_open(&handle, 0);
	snd_mixer_attach(handle, "default");
	snd_mixer_selem_register(handle, NULL, NULL);
	snd_mixer_load(handle);

	snd_mixer_selem_id_alloca(&sid);
	snd_mixer_selem_id_set_name(sid, element_name);
	elem = snd_mixer_find_selem(handle, sid);

	if (elem) {
		snd_mixer_selem_set_playback_volume_all(elem, volume);
	} else {
		snd_mixer_close(handle);
		return -1;
	}

	snd_mixer_close(handle);
	return 0;
}

int main() {
  int r, num;
  char buf[64];

	// load config file
	char button_cmds[MAX_COMMANDS][MAX_LINE];
	char enc_cmds_cw[MAX_COMMANDS][MAX_LINE];
	char enc_cmds_cw_pressed[MAX_COMMANDS][MAX_LINE];
	char enc_cmds_ccw[MAX_COMMANDS][MAX_LINE];
	char enc_cmds_ccw_pressed[MAX_COMMANDS][MAX_LINE];

	char config_path[512];
	snprintf(config_path, sizeof(config_path), "%s/.config/macropad/config.ini", getenv("HOME"));

	load_config(config_path, button_cmds, "button_cmds");
	load_config(config_path, enc_cmds_cw, "enc_cmds_cw");
	load_config(config_path, enc_cmds_cw_pressed, "enc_cmds_cw_pressed");
	load_config(config_path, enc_cmds_ccw, "enc_cmds_ccw");
	load_config(config_path, enc_cmds_ccw_pressed, "enc_cmds_ccw_pressed");

  while (1) {
		r = rawhid_open(1, 0x16C0, 0x0480, 0xFFAB, 0x0200);
		if (r <= 0) {
			r = rawhid_open(1, 0x16C0, 0x0486, 0xFFAB, 0x0200);
			if (r <= 0) {
				printf("no rawhid device found.\n");
				exit(0);
			}
		}
		printf("found rawhid device\n");

		while (1) {
			// read rawhid data
			num = rawhid_recv(0, buf, 64, 220);
			if (num < 0) {
				printf("\nerror reading, device went offline, exiting...\n");
				rawhid_close(0);
				exit(0);
			}
			if (num > 0) {
				unsigned long value;
				char idx = 0, dir = 0, btn = 0;
				while (idx < num) {
					switch (buf[(int)idx]) {
					case 0x01:
						// read master volume
						value = (uint16_t)(((unsigned char)buf[idx + 1] << 8) | (unsigned char)buf[idx + 2]);
						set_volume("Master", value * 16);
						idx += 3;
						break;
					case 0x02:
						// read capture volume
						value = (uint16_t)(((unsigned char)buf[idx + 1] << 8) | (unsigned char)buf[idx + 2]);
						set_volume("Capture", value * 16);
						idx += 3;
						break;
					case 0x03:
						// button press
						value = buf[idx + 1];
						system(button_cmds[value]);
						idx += 2;
						break;
					case 0x04:
						// encoder rotation
						dir = buf[idx + 1];
						value = buf[idx + 2];
						btn = buf[idx + 3];
						if (btn) {
							if (dir)
								system(enc_cmds_cw_pressed[value]);
							else
								system(enc_cmds_ccw_pressed[value]);
						} else {
							if (dir)
								system(enc_cmds_cw[value]);
							else
								system(enc_cmds_ccw[value]);
						}
						idx += 4;
						break;
					default:
						idx++;
						break;
					}
				}
			}
		}
	}
}

