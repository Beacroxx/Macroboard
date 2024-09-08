#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>
#include <ctype.h>

#define MAX_LINE 256
#define MAX_COMMANDS 10

#define run_command(cmd) system(cmd)

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
		printf("Error opening file %s\n Does it exist?\n", filename);
		exit(1);
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
		snd_mixer_selem_set_capture_volume_all(elem, volume);
	} else {
		snd_mixer_close(handle);
		return -1;
	}

	snd_mixer_close(handle);
	return 0;
}

int connect_to_midi_device(snd_seq_t *seq, int port) {
    snd_seq_addr_t sender, dest;
    snd_seq_port_info_t *pinfo;
    snd_seq_port_info_alloca(&pinfo);
    int client;

    for (client = 0; client < 128; ++client) {
        if (snd_seq_get_any_client_info(seq, client, pinfo) >= 0) {
            const char *name = snd_seq_client_info_get_name(pinfo);
            if (strstr(name, "Teensy") != NULL) {  // Replace with your device's name
                sender.client = client;
                sender.port = 0;  // Assuming it's the first port, adjust if needed
                break;
            }
        }
    }

    if (client == 128) {
        return 0;  // Device not found
    }

    dest.client = snd_seq_client_id(seq);
    dest.port = port;

    // Disconnect existing connections
    snd_seq_disconnect_from(seq, port, sender.client, sender.port);

    if (snd_seq_connect_from(seq, port, sender.client, sender.port) < 0) {
        return 0;  // Cannot connect
    }

    return 1;  // Successfully connected
}

int main() {
	snd_seq_t *seq;
	int port;

	// load config file
	char button_cmds[MAX_COMMANDS][MAX_LINE];
	char enc_cmds_cw[MAX_COMMANDS][MAX_LINE];
	char enc_cmds_cw_pressed[MAX_COMMANDS][MAX_LINE];
	char enc_cmds_ccw[MAX_COMMANDS][MAX_LINE];
	char enc_cmds_ccw_pressed[MAX_COMMANDS][MAX_LINE];
	char fader_left[MAX_COMMANDS][MAX_LINE];
	char fader_right[MAX_COMMANDS][MAX_LINE];

	// buttons for encoders
	char enc_buttons[5];

	char config_path[512];
	snprintf(config_path, sizeof(config_path), "%s/.config/macropad/config.ini", getenv("HOME"));

	load_config(config_path, button_cmds, "button_cmds");
	load_config(config_path, enc_cmds_cw, "enc_cmds_cw");
	load_config(config_path, enc_cmds_cw_pressed, "enc_cmds_cw_pressed");
	load_config(config_path, enc_cmds_ccw, "enc_cmds_ccw");
	load_config(config_path, enc_cmds_ccw_pressed, "enc_cmds_ccw_pressed");
	load_config(config_path, fader_left, "fader_left");
	load_config(config_path, fader_right, "fader_right");

   // Open ALSA sequencer
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
        fprintf(stderr, "Error opening ALSA sequencer.\n");
        exit(1);
    }

    snd_seq_set_client_name(seq, "MIDI Macropad Driver");

    // Create a port for our application
    if ((port = snd_seq_create_simple_port(seq, "MIDI Macropad Port",
        SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE|
        SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ,
        SND_SEQ_PORT_TYPE_APPLICATION)) < 0) {
        fprintf(stderr, "Error creating sequencer port.\n");
        snd_seq_close(seq);
        exit(1);
    }

    if (!connect_to_midi_device(seq, port)) {
        fprintf(stderr, "Failed to connect to MIDI device.\n");
        snd_seq_close(seq);
        exit(1);
    }

    printf("Connected to MIDI device. Listening on port %d\n", port);

	int msb_left = 0;
	int msb_right = 0;
  while (1) {
		snd_seq_event_t *ev;
        
		int err = snd_seq_event_input_pending(seq, 1);

		if (err == 0) {
			if (!connect_to_midi_device(seq, port)) {
				fprintf(stderr, "MIDI device disconnected.\n");
				snd_seq_close(seq);
				exit(0);
			}
		}

		// Wait for MIDI event
		snd_seq_event_input(seq, &ev);
		switch (ev->type) {
			case SND_SEQ_EVENT_CONTROLLER:
				if (ev->data.control.param == 1) {
						// Left Fader MSB
						msb_left = ev->data.control.value;
				} else if (ev->data.control.param == 2) {
						// Left Fader LSB
						int value_left = (msb_left << 7) | ev->data.control.value;
						set_volume(fader_left[0], value_left * 4);
				} else if (ev->data.control.param == 3) {
						// Right Fader MSB
						msb_right = ev->data.control.value;
				} else if (ev->data.control.param == 4) {
						// Right Fader LSB
						int value_right = (msb_right << 7) | ev->data.control.value;
						set_volume(fader_right[0], value_right * 4);
				} else if (ev->data.control.param >= 5 && ev->data.control.param < 15) {
					// Encoders
					int index = ev->data.control.param - 5;
					char pressed = enc_buttons[index];
					if (ev->data.control.value == 65) {
						// CW
						if (pressed == 1) {
							run_command(enc_cmds_cw_pressed[index]);
						} else {
							run_command(enc_cmds_cw[index]);
						}
					} else if (ev->data.control.value == 63) {
						// CCW
						if (pressed == 1) {
							run_command(enc_cmds_ccw_pressed[index]);
						} else {
							run_command(enc_cmds_ccw[index]);
						}
					}
				}

				break;

		  case SND_SEQ_EVENT_NOTEON:
				if (ev->data.note.note >= 60 && ev->data.note.note < 70) {
					run_command(button_cmds[ev->data.note.note - 60]);
				} else if (ev->data.note.note >= 70 && ev->data.note.note < 75) {
					int encoder = ev->data.note.note - 70;
					enc_buttons[encoder] = 1;
				}

				break;

		  case SND_SEQ_EVENT_NOTEOFF:
				if (ev->data.note.note >= 70 && ev->data.note.note < 75) {
					int encoder = ev->data.note.note - 70;
					enc_buttons[encoder] = 0;
				}

				break;

		  default:
				break;
		}
	}
}
