#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <cstdlib>
#include <memory>
#include <alsa/asoundlib.h>

#define MAX_LINE 256
#define MAX_COMMANDS 10

// Run a system command
void run_command(const std::string& cmd) {
    system(cmd.c_str());
}

// Trim whitespace from a string
void trim(std::string& str) {
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    str.erase(std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), str.end());
}

// Load a section of commands from a file
void load_config(const std::string& filename, std::array<std::string, MAX_COMMANDS>& commands, const std::string& section) {
    std::ifstream file(filename);
    if (!file) {
        std::cerr << "Error opening file " << filename << "\nDoes it exist?\n";
        exit(1);
    }

    std::string line;
    bool in_section = false;
    int command_index = 0;

    while (std::getline(file, line)) {
        trim(line);

        if (line[0] == '[') {
            std::string section_name = line.substr(1, line.find(']') - 1);
            in_section = (section_name == section);
            continue;
        }

        if (in_section && command_index < MAX_COMMANDS) {
            size_t equals_pos = line.find('=');
            if (equals_pos != std::string::npos) {
                std::string value = line.substr(equals_pos + 1);
                trim(value);
                commands[command_index++] = value;
            }
        }
    }
}

// Set the volume of a mixer element
snd_mixer_t *handle;
snd_mixer_elem_t *elem;
snd_mixer_selem_id_t *sid;
int set_volume(const std::string& element_name, long volume) {
    snd_mixer_selem_id_set_name(sid, element_name.c_str());
    elem = snd_mixer_find_selem(handle, sid);


    if (elem) {
        snd_mixer_selem_set_playback_volume_all(elem, volume);
        snd_mixer_selem_set_capture_volume_all(elem, volume);
    } else {
        return -1;
    }

    return 0;
}

int connect_to_midi_device(snd_seq_t *seq, int port) {
    snd_seq_addr_t sender, dest;
    snd_seq_client_info_t *cinfo;
    snd_seq_port_info_t *pinfo;
    snd_seq_client_info_alloca(&cinfo);
    snd_seq_port_info_alloca(&pinfo);
    int client;

    snd_seq_client_info_set_client(cinfo, -1);
    while (snd_seq_query_next_client(seq, cinfo) >= 0) {
        client = snd_seq_client_info_get_client(cinfo);
        const char *name = snd_seq_client_info_get_name(cinfo);
        if (std::string(name).find("Teensy") != std::string::npos) {  // Replace with your device's name
            sender.client = client;
            
            // Find the first input port of this client
            snd_seq_port_info_set_client(pinfo, client);
            snd_seq_port_info_set_port(pinfo, -1);
            while (snd_seq_query_next_port(seq, pinfo) >= 0) {
                if (snd_seq_port_info_get_capability(pinfo) & SND_SEQ_PORT_CAP_READ) {
                    sender.port = snd_seq_port_info_get_port(pinfo);
                    break;
                }
            }
            
            break;
        }
    }

    if (snd_seq_client_info_get_client(cinfo) == -1) {
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
    std::array<std::string, MAX_COMMANDS> button_cmds;
    std::array<std::string, MAX_COMMANDS> enc_cmds_cw;
    std::array<std::string, MAX_COMMANDS> enc_cmds_cw_pressed;
    std::array<std::string, MAX_COMMANDS> enc_cmds_ccw;
    std::array<std::string, MAX_COMMANDS> enc_cmds_ccw_pressed;
    std::array<std::string, MAX_COMMANDS> fader_left;
    std::array<std::string, MAX_COMMANDS> fader_right;

    // buttons for encoders
    std::array<char, 5> enc_buttons{};

    std::string config_path = std::string(getenv("HOME")) + "/.config/macropad/config.ini";

    load_config(config_path, button_cmds, "button_cmds");
    load_config(config_path, enc_cmds_cw, "enc_cmds_cw");
    load_config(config_path, enc_cmds_cw_pressed, "enc_cmds_cw_pressed");
    load_config(config_path, enc_cmds_ccw, "enc_cmds_ccw");
    load_config(config_path, enc_cmds_ccw_pressed, "enc_cmds_ccw_pressed");
    load_config(config_path, fader_left, "fader_left");
    load_config(config_path, fader_right, "fader_right");

    // Open ALSA sequencer
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
        std::cerr << "Error opening ALSA sequencer.\n";
        exit(1);
    }

    snd_seq_set_client_name(seq, "MIDI Macropad Driver");

    // Create a port for our application
    if ((port = snd_seq_create_simple_port(seq, "MIDI Macropad Port",
        SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE|
        SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ,
        SND_SEQ_PORT_TYPE_APPLICATION)) < 0) {
        std::cerr << "Error creating sequencer port.\n";
        snd_seq_close(seq);
        exit(1);
    }

    if (!connect_to_midi_device(seq, port)) {
        std::cerr << "Failed to connect to MIDI device.\n";
        snd_seq_close(seq);
        exit(1);
    }

    std::cout << "Connected to MIDI device. Listening on port " << port << std::endl;

    int msb_left = 0;
    int msb_right = 0;

    // Open ALSA mixer
    snd_mixer_open(&handle, 0);
    snd_mixer_attach(handle, "default");
    snd_mixer_selem_register(handle, NULL, NULL);
    snd_mixer_load(handle);
    snd_mixer_selem_id_alloca(&sid);

    while (true) {
        snd_seq_event_t *ev;

        int err = snd_seq_event_input_pending(seq, 1);

        if (err == 0) {
            if (!connect_to_midi_device(seq, port)) {
                std::cerr << "MIDI device disconnected.\n";
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

        if (ev != nullptr) {
        }

        snd_seq_free_event(ev);
    }

    snd_seq_close(seq);
    snd_mixer_close(handle);

    return 0;
}