#include <allonet/allonet.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <cJSON/cJSON.h>
#include "../../src/util.h"
#include <enet/enet.h>
#ifndef MIN
#   define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#endif

#define MODIFY_TERMINAL 0

#if MODIFY_TERMINAL == 1
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
struct termios orig_termios;

void reset_terminal_mode()
{
    tcsetattr(0, TCSANOW, &orig_termios);
}

void set_conio_terminal_mode()
{
    struct termios new_termios;

    /* take two copies - one for now, one for later */
    tcgetattr(0, &orig_termios);
    memcpy(&new_termios, &orig_termios, sizeof(new_termios));

    /* register cleanup handler, and set the new terminal mode */
    atexit(reset_terminal_mode);
    cfmakeraw(&new_termios);
    tcsetattr(0, TCSANOW, &new_termios);
}
int kbhit()
{
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv);
}

int getch()
{
    int r;
    unsigned char c;
    if ((r = read(0, &c, sizeof(c))) < 0) {
        return r;
    } else {
        return c;
    }
}

#endif

static const char *me;
static int32_t track_id = 0;

allo_client_intent* intent;

static void asset_state(alloclient *client, const char *asset_id, int state) {
    printf("Asset %s changed state %d\n", asset_id, state);
}

static bool interaction(
    alloclient *client, 
    allo_interaction *inter
)
{
    cJSON *body = cJSON_Parse(inter->body);
    const char *interaction_name = cJSON_GetArrayItem(body, 0)->valuestring;
    
    if(strcmp(interaction_name, "point") == 0 ) {
        return true;
    }

    printf(
        "INTERACTION\n\tType: %s\n\tSender: %s\n\tReceiver: %s\n\tID: %s\n\tBody: %s\n", 
        inter->type, inter->sender_entity_id, inter->receiver_entity_id, inter->request_id, inter->body
    );
    if(strcmp(interaction_name, "announce") == 0) {
        me = strdup(cJSON_GetArrayItem(body, 1)->valuestring);
        allo_interaction *request = allo_interaction_create("request", me, "place", "123", "[\"lol\", 1, 2, 3]");
        alloclient_send_interaction(client, request);
        allo_interaction_free(request);

        request = allo_interaction_create("request", me, "place", "124", "[\"allocate_track\", \"audio\", 4800, 1, \"opus\"]");
        alloclient_send_interaction(client, request);
        allo_interaction_free(request);
    }
    
    if(strcmp(interaction_name, "poke") == 0 ) {
        bool mousedown = cJSON_IsTrue(cJSON_GetArrayItem(body, 1));
        if(mousedown) {
            intent->zmovement = !intent->zmovement;
            alloclient_set_intent(client, intent);
            allo_interaction *response = allo_interaction_create("response", me, inter->sender_entity_id, inter->request_id, "[\"poke\", \"ok\"]");
            alloclient_send_interaction(client, response);
            allo_interaction_free(response);
        }
    }

    if(strcmp(interaction_name, "allocate_track") == 0) {
        track_id = cJSON_GetArrayItem(body, 2)->valueint;
    }

    cJSON_Delete(body);
    return true;
}

static cJSON *cvec2(float u, float v)
{
  return cjson_create_list(cJSON_CreateNumber(u), cJSON_CreateNumber(v), NULL);
}

static cJSON *cvec3(float x, float y, float z)
{
  return cjson_create_list(cJSON_CreateNumber(x), cJSON_CreateNumber(y), cJSON_CreateNumber(z), NULL);
}

static void disconnected(alloclient *client, alloerror code, const char *reason)
{
    printf("Disconnected: %d/%s\n", code, reason);
    alloclient_disconnect(client, 0);
    exit(1);
}

// generate 10ms of audio every 10ms at most
static enet_uint32 last = 0;
static double fnow = 0;
static uint16_t* audio;
static size_t audio_len;
static size_t audio_cursor;
static bool play_file = true;
static void send_audio(alloclient *client)
{
    enet_uint32 now = enet_time_get();
    if (track_id == 0 || now-last < 10)
    {
        return;
    }
    last = now;

	if (audio == NULL && play_file) {
		FILE* fp = fopen("audio.pcm", "r");
		if (fp == NULL) {
            play_file = false;
            perror("can't fopen");
            return;
        }
        if (fseek(fp, 0L, SEEK_END) != 0) {
            perror("Error fseeking audio to end");
            play_file = false;
            return;
        }
        audio_len = ftell(fp)/sizeof(uint16_t);
        if (audio_len == -1) { perror("can't ftell"); play_file = false;  return; }
        audio = calloc(audio_len, sizeof(uint16_t));
        if (fseek(fp, 0L, SEEK_SET) != 0) { perror("can't fseek");  play_file = false;  return; }
        size_t newLen = fread(audio, sizeof(int16_t), audio_len, fp);
        if (ferror(fp) != 0) {
            perror("Error freading audio");
            play_file = false;
            return;
        }
        fclose(fp);
        fprintf(stderr, "Opened audio file\n");
	}
    
	int16_t pcm[960];
	if (play_file && audio) {
		size_t len = 960;
		int16_t* dest = pcm;
		while (len > 0) {
			size_t this_len = MIN(len, audio_len - audio_cursor);
			memcpy(dest, audio + audio_cursor, this_len * sizeof(int16_t));
			len -= this_len;
			audio_cursor += this_len;
			dest += this_len;
			if (audio_cursor >= audio_len - 1) {
				audio_cursor = 0;
                printf("Bleep bloop!\n");
			}
		}
	} else {
		double time_per_sample = 1 / 48000.0;
		for (int i = 0; i < 960; i++)
		{
			pcm[i] = sin(fnow * 440 * 3.141592*2) * INT16_MAX * 0.5;
			fnow += time_per_sample;
		}
	}
	alloclient_send_audio(client, track_id, pcm, 960);
}


int main(int argc, char **argv)
{
    if(!allo_initialize(false)) {
        fprintf(stderr, "Unable to initialize allonet");
        return -1;
    }

    if(argc != 3) {
        fprintf(stderr, "Usage: allodummyclient username alloplace://hostname:port\n");
        return -2;
    }

    intent = allo_client_intent_create();

    printf("hello microverse\n");

    cJSON *avatardesco = cjson_create_object(
        "children", cjson_create_list(
            cjson_create_object(
                "geometry", cjson_create_object(
                    "type", cJSON_CreateString("hardcoded-model"),
                    "name", cJSON_CreateString("head"),
                    NULL
                ),
                "intent", cjson_create_object(
                    "actuate_pose", cJSON_CreateString("head"),
                    NULL
                ),
                NULL
            ),
            NULL
        ),
        "collider", cjson_create_object(
            "type", cJSON_CreateString("box"),
            "width", cJSON_CreateNumber(1),
            "height", cJSON_CreateNumber(1),
            "depth", cJSON_CreateNumber(1),
            NULL
        ),
        "geometry", cjson_create_object(
            "type", cJSON_CreateString("inline"),
            "vertices", cjson_create_list(
                cvec3(0, 0, 0),
                cvec3(1, 0, 0),
                cvec3(1, 1, 0),
                cvec3(0, 1, 0),
                cvec3(1, 1, 1),
                NULL
            ),
            /*"normals", cjson_create_list(
                cvec3(0,0,0),
                cvec3(0,0,0),
                cvec3(0,0,0),
                cvec3(0,0,0),
                cvec3(0,0,0),
                NULL
            ),*/
            "uvs", cjson_create_list(
                cvec2(0, 0),
                cvec2(1, 0),
                cvec2(1, 1),
                cvec2(0, 1),
                cvec2(1, 1),
                NULL
            ),
            "triangles", cjson_create_list(
                cvec3(0, 1, 2), cvec3(0, 2, 3), cvec3(4, 0, 1), cvec3(4, 2, 1),
                NULL
            ),
            "texture", cJSON_CreateString("iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAAAXNSR0IArs4c6QAAAAlwSFlzAAAOxAAADsQBlSsOGwAAAVlpVFh0WE1MOmNvbS5hZG9iZS54bXAAAAAAADx4OnhtcG1ldGEgeG1sbnM6eD0iYWRvYmU6bnM6bWV0YS8iIHg6eG1wdGs9IlhNUCBDb3JlIDUuNC4wIj4KICAgPHJkZjpSREYgeG1sbnM6cmRmPSJodHRwOi8vd3d3LnczLm9yZy8xOTk5LzAyLzIyLXJkZi1zeW50YXgtbnMjIj4KICAgICAgPHJkZjpEZXNjcmlwdGlvbiByZGY6YWJvdXQ9IiIKICAgICAgICAgICAgeG1sbnM6dGlmZj0iaHR0cDovL25zLmFkb2JlLmNvbS90aWZmLzEuMC8iPgogICAgICAgICA8dGlmZjpPcmllbnRhdGlvbj4xPC90aWZmOk9yaWVudGF0aW9uPgogICAgICA8L3JkZjpEZXNjcmlwdGlvbj4KICAgPC9yZGY6UkRGPgo8L3g6eG1wbWV0YT4KTMInWQAAAttJREFUOBFtU0tsjFEUPvfx//O3TJ+mbToe1RSRSJQfRYqJZFTSxkYqsbBgMUQ0jddCBLMRQbGxaVhgIRGWFiIaDYsmfaALbUiakopH+phW6Mz8/3049zczunCSm3PuOd+553kBchSLJXleNlxrTSD2kidjwAM5Z9TJGG8HYAuxNJbUBef6tpvH6luvHVkIMPKZjXWHDzRUteT1gwnXQpmQvAL5Drej71bFyq0uKAW/Jgb6+obfnDrVezwbb2++3VgT3j6X8eDtt9knB58OnUf8R+NrHihbd+jRjSVrWo4UlZWCEuArTFBlwZob6IL4zBXoWL0NFtvaz2rCihilo1Nz6tXnySune0cvk8bE83e1bny9TINUGnTGk5x9eQ3R711+JJy25lk1ZFJT/n6WtjZVONqxbaEBeNjm5PHIxAvulC6txmgghQJveoxHPt2BGvoQ7PItlq/C2kFjqKLcepBeBIM/Zsm+sLCWhh0/K5UVLSleRrXy0xRTznhZEh27ACuKe4AtioOQHKj2iCKUgJRQ53D4VFkFd1NaCyEIVgKekFnTfQtTwrkpsIqqQJEMaKyHEKwHqLEEvfawvlIMFApZROrAAzHEooSwDCiDQqXykePB4fyFGP0/kgaCDxkyDUNvoCKdcngxAGW2h1ngU2YwCDJRCifwCSySUIVRRdixIC1lhAkpRuzi+s28pLZmSaqHOHTSl9rBHFRhRzBVs5kaGPMd4fGtjuL9X1Mf7r8ZP5EHkVD8+sk9of7OhlqxXJMQ9k346GO2zVQkKGWUcU6nP0/M14y/v3R1ZKYrsLlutzU0dNQUb4g1N204W7cscrE6UlmE3VYYV1g2t3/+/A0TX77de9Y7cA5x3w242w3W2YhA3MRgEC24AUT37m56kDi0T3cm2vX+tl3DqN+Zs0F37h/k7ws5Sfw1BrpV9dHWJnft8TwgiT8R5dxs89r/c5YDF6yxWOBcuOeFP4paJwh7m0NPAAAAAElFTkSuQmCC"),
            NULL
        ),
        NULL
    );
    const char *avatardesc = cJSON_Print(avatardesco);

    char *identity = (char*)calloc(1, 255);
    snprintf(identity, 255, "{\"display_name\": \"%s\"}", argv[1]);
    alloclient *client = alloclient_create(true);
    alloclient_connect(client, argv[2], identity, avatardesc);
    cJSON_Delete(avatardesco);
    free((void*)avatardesc);

    if(!client) {
        return -3;
    }

#if MODIFY_TERMINAL == 1
    set_conio_terminal_mode();
#endif
    client->interaction_callback = interaction;
    client->disconnected_callback = disconnected;
    client->asset_state_callback = asset_state;
    
    int i = 0;
    
    
    for(;;)
    {
#if MODIFY_TERMINAL == 1
        if(kbhit()) {
            int ch = getch();
            allo_client_intent intent = {
                .zmovement = ch=='w' ? 1 : ch == 's' ? -1 : 0,
                .xmovement = ch=='d' ? 1 : ch == 'a' ? -1 : 0,
            };
            alloclient_set_intent(client, intent);
        }
#endif
        alloclient_poll(client, 10);

        if( i++ % 100)
        {
            if(intent->zmovement)
            {
                intent->yaw += 0.01;
            }
            allo_client_poses poses = {
                .left_hand = {
                    .matrix = allo_m4x4_concat(
                        allo_m4x4_rotate(-3.14 / 2, (allo_vector) { 0,0,1 }),
                        allo_m4x4_translate((allo_vector) { -1, 0, 1 })
                    )
                },
                .head = {
                    .matrix = allo_m4x4_translate((allo_vector) { 0, 0, 2 })
                }
            };
            memcpy(&intent->poses, &poses, sizeof(poses));
            alloclient_set_intent(client, intent);
        }
        
        send_audio(client);
    }
    return 0;
}
