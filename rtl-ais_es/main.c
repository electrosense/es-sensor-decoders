/*
 * Copyright (C) 2012 by Kyle Keen <keenerd@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Sockets */
#include <sys/socket.h>
#include <sys/un.h>
#include <error.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <arpa/inet.h> 
#include <netinet/in.h> 
#define OUTPUTPORT 5555
#define OUTPUTBUFFER 16384

/* RTL_AIS */
typedef void* rtlsdr_dev_t;
#include "convenience.h"
#include "rtl_ais.h"

/* AIS Parser */
#include "portable.h"
#include "nmea.h"
#include "sixbit.h"
#include "vdm_parse.h"

void usage(void)
{
	fprintf(stderr,
			"rtl_ais, a simple AIS tuner\n"
			"\t and generic dual-frequency FM demodulator\n\n"
			"(probably not a good idea to use with e4000 tuners)\n"
			"Use: rtl_ais [options] [outputfile]\n"
			"\t[-l left_frequency (default: 161.975M)]\n"
			"\t[-r right_frequency (default: 162.025M)]\n"
			"\t    left freq < right freq\n"
			"\t    frequencies must be within 1.2MHz\n"
			"\t[-s sample_rate (default: 24k)]\n"
			"\t    maximum value, might be down to 12k\n"
			"\t[-o output_rate (default: 48k)]\n"
			"\t    must be equal or greater than twice -s value\n"
			"\t[-E toggle edge tuning (default: off)]\n"
			"\t[-D toggle DC filter (default: on)]\n"
			//"\t[-O toggle oversampling (default: off)\n"
			"\t[-d device_index (default: 0)]\n"
			"\t[-g tuner_gain (default: automatic)]\n"
			"\t[-p ppm_error (default: 0)]\n"
			"\t[-R enable RTL chip AGC (default: off)]\n"
			"\t[-A turn off built-in AIS decoder (default: on)]\n"
			"\t    use this option to output samples to file or stdout.\n"
			"\tBuilt-in AIS decoder options:\n"
			"\t[-h host (default: 127.0.0.1)]\n"
			"\t[-P port (default: 10110)]\n"
			"\t[-T use TCP communication, rtl-ais is tcp server ( -h is ignored)\n"
			"\t[-t time to keep ais messages in sec, using tcp listener (default: 15)\n"
			"\t[-n log NMEA sentences to console (stderr) (default off)]\n"
			"\t[-L log sound levels to console (stderr) (default off)]\n\n"
			"\t[-S seconds_for_decoder_stats (default 0=off)]\n\n"
			"\tWhen the built-in AIS decoder is disabled the samples are sent to\n"
			"\tto [outputfile] (a '-' dumps samples to stdout)\n"
			"\t    omitting the filename also uses stdout\n\n"
			"\tOutput is stereo 2x16 bit signed ints\n\n"
			"\tExamples:\n"
			"\tReceive AIS traffic,sent UDP NMEA sentences to 127.0.0.1 port 10110\n"
			"\t     and log the senteces to console:\n\n"
			"\trtl_ais -n\n\n"
			"\tTune two fm stations and play one on each channel:\n\n"
			"\trtl_ais -l233.15M  -r233.20M -A  | play -r48k -traw -es -b16 -c2 -V1 - "
			"\n");
	exit(1);
}

static volatile int do_exit = 0;
static void sighandler(int signum)
{
	signum = signum;
	fprintf(stderr, "Signal caught, exiting!\n");
	do_exit = 1;
}

typedef struct ais_entity_s {
	int mmsi;
	char date[25];
	char name[25];
	char callsign[25];
	char latitude[25];
	char longitude[25];
	int true;
	char dest[25];
	struct ais_entity_s *next;
} ais_entity;

ais_entity* find_and_create_ais(ais_entity **head, int mmsi) {
	ais_entity *new;
	ais_entity *pos=*head;
	/* Hashes overruled */
	while(pos) {
		if(pos->mmsi==mmsi)
			return pos;
		pos=pos->next;
	}
	new = malloc(sizeof(ais_entity));
	memset(new, 0, sizeof(ais_entity));
	new->mmsi=mmsi;
	new->true=-1000;
	new->next=*head;
	*head=new;
	return new;
}

int main(int argc, char **argv)
{

	/* AIS Parser structures */
	ais_state ais;
	char buf[256];
	/* AIS message structures, only parse ones with positions */
	aismsg_1 msg_1;
	aismsg_2 msg_2;
	aismsg_3 msg_3;
	aismsg_4 msg_4;
	aismsg_5 msg_5;
	aismsg_9 msg_9;
	aismsg_11 msg_11;
	aismsg_18 msg_18;
	aismsg_19 msg_19;
	aismsg_20 msg_20;

	ais_entity *ais_list = NULL;

	/* Hack */
	char end_buffer[OUTPUTBUFFER];

	/* Position in DD.DDDDDD */
	double lat_dd;
	double long_ddd;
	long userid;
	char *name;
	timetag timestamp;

	/* Open output socket */
	int output_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (output_socket < 0)
        error(EXIT_FAILURE, errno, "Output socket failed");
    printf("Opened output socket\n");
	struct sockaddr_in servaddr;
 	memset(&servaddr, 0, sizeof(servaddr));
    // Filling server information 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_port = htons(OUTPUTPORT); 
    servaddr.sin_addr.s_addr = INADDR_ANY; 



	signal(SIGINT, sighandler);
	signal(SIGTERM, sighandler);
	int opt;

	struct rtl_ais_config config;
	rtl_ais_default_config(&config);


	while ((opt = getopt(argc, argv, "l:r:s:o:EOD:g:p:RATt:P:h:nLS:?")) != -1)
	{
		switch (opt)
		{
		case 'l':
			config.left_freq = (int)atofs(optarg);
			break;
		case 'r':
			config.right_freq = (int)atofs(optarg);
			break;
		case 's':
			config.sample_rate = (int)atofs(optarg);
			break;
		case 'o':
			config.output_rate = (int)atofs(optarg);
			break;
		case 'E':
			config.edge = !config.edge;
			break;
		case 'D':
			config.dc_filter = !config.dc_filter;
			break;
		case 'O':
			config.oversample = !config.oversample;
			break;
		case 'g':
			config.gain = (int)(atof(optarg) * 10);
			break;
		case 'p':
			config.ppm_error = atoi(optarg);
			config.custom_ppm = 1;
			break;
		case 'R':
			config.rtl_agc = 1;
			break;
		case 'A':
			config.use_internal_aisdecoder = 0;
			break;
		case 'P':
			config.port = strdup(optarg);
			break;
		case 'T':
			config.use_tcp_listener = 1;
			break;
		case 't':
			config.tcp_keep_ais_time = atoi(optarg);
			break;
		case 'h':
			config.host = strdup(optarg);
			break;
		case 'L':
			config.show_levels = 1;
			break;
		case 'S':
			config.seconds_for_decoder_stats = atoi(optarg);
			break;
		case 'n':
			config.debug_nmea = 1;
			break;
		case '?':
		default:
			usage();
			return 2;
		}
	}

	if (argc <= optind)
	{
		config.filename = "-";
	}
	else
	{
		config.filename = argv[optind];
	}

	if (config.edge)
	{
		fprintf(stderr, "Edge tuning enabled.\n");
	}
	else
	{
		fprintf(stderr, "Edge tuning disabled.\n");
	}
	if (config.dc_filter)
	{
		fprintf(stderr, "DC filter enabled.\n");
	}
	else
	{
		fprintf(stderr, "DC filter disabled.\n");
	}
	if (config.rtl_agc)
	{
		fprintf(stderr, "RTL AGC enabled.\n");
	}
	else
	{
		fprintf(stderr, "RTL AGC disabled.\n");
	}
	if (config.use_internal_aisdecoder)
	{
		fprintf(stderr, "Internal AIS decoder enabled.\n");
	}
	else
	{
		fprintf(stderr, "Internal AIS decoder disabled.\n");
	}

	struct rtl_ais_context *ctx = rtl_ais_start(&config);
	if (!ctx)
	{
		fprintf(stderr, "\nrtl_ais_start failed, exiting...\n");
		exit(1);
	}
	/* 
	  aidecoder.c appends the messages to a queue that can be used for a 
	  routine if rtl_ais is compiled as lib. Here we only loop and dequeue
	  the messages, and the puts() sentence that print the message is  
	  commented out. If the -n parameter is used the messages are printed from 
	  nmea_sentence_received() in aidecoder.c 
	  */

	/* Clear out the structures */
	memset(&ais, 0, sizeof(ais_state));

	while (!do_exit && rtl_ais_isactive(ctx))
	{
		const char *str;
		if (config.use_internal_aisdecoder)
		{
			// dequeue
			while ((str = rtl_ais_next_message(ctx)))
			{
				/* Init output string */
				end_buffer[0]=0;
				strcat(end_buffer, "\3{\"AISMessage\":\"");
				strcat(end_buffer, str);
				/* Remove last \r\n */
				end_buffer[strlen(end_buffer)-2]=0;
				strcat(end_buffer, "\"}");
				sendto(output_socket, (const char *)end_buffer, strlen(end_buffer), 
					MSG_CONFIRM, (const struct sockaddr *) &servaddr,  
					sizeof(servaddr)); 
				if (assemble_vdm(&ais, str) == 0)
				{
					/* Get the 6 bit message id */
					ais.msgid = (unsigned char)get_6bit(&ais.six_state, 6);
					/* process message with appropriate parser */
					switch (ais.msgid)
					{
					case 1:
						if (parse_ais_1(&ais, &msg_1) == 0)
						{
							userid = msg_1.userid;
							pos2ddd(msg_1.latitude, msg_1.longitude, &lat_dd, &long_ddd);
							ais_entity* entity=find_and_create_ais(&ais_list, userid);
							snprintf(entity->true, 24, "%d", msg_2.true);
							snprintf(entity->latitude, 24, "%0.6f", lat_dd);
							snprintf(entity->longitude, 24, "%0.6f", long_ddd);
						}
						break;

					case 2:
						if (parse_ais_2(&ais, &msg_2) == 0)
						{
							userid = msg_2.userid;
							pos2ddd(msg_2.latitude, msg_2.longitude, &lat_dd, &long_ddd);
							ais_entity* entity=find_and_create_ais(&ais_list, userid);
							snprintf(entity->true, 24, "%d", msg_2.true);
							snprintf(entity->latitude, 24, "%0.6f", lat_dd);
							snprintf(entity->longitude, 24, "%0.6f", long_ddd);
						}
						break;

					case 3:
						if (parse_ais_3(&ais, &msg_3) == 0)
						{
							userid = msg_3.userid;
							pos2ddd(msg_3.latitude, msg_3.longitude, &lat_dd, &long_ddd);
							ais_entity* entity=find_and_create_ais(&ais_list, userid);
							snprintf(entity->true, 24, "%d", msg_2.true);
							snprintf(entity->latitude, 24, "%0.6f", lat_dd);
							snprintf(entity->longitude, 24, "%0.6f", long_ddd);
						}
						break;

					case 4:
						if (parse_ais_4(&ais, &msg_4) == 0)
						{
							userid = msg_4.userid;
							pos2ddd(msg_4.latitude, msg_4.longitude, &lat_dd, &long_ddd);
							ais_entity* entity=find_and_create_ais(&ais_list, userid);
							snprintf(entity->latitude, 24, "%0.6f", lat_dd);
							snprintf(entity->longitude, 24, "%0.6f", long_ddd);
							snprintf(entity->date, 24, "%04u-%02u-%02u %02u:%02u:%02u",
							 	msg_4.utc_year, msg_4.utc_month, msg_4.utc_day,
							  	msg_4.utc_hour, msg_4.utc_minute, msg_4.utc_second);						
							}
						break;

					case 5:
						if (parse_ais_5(&ais, &msg_5) == 0)
						{
							userid = msg_5.userid;
							ais_entity* entity=find_and_create_ais(&ais_list, userid);
							strncpy(entity->name, 24, msg_5.name);
							strncpy(entity->callsign, 24, msg_5.callsign);
							strncpy(entity->dest, 24, msg_5.dest);
						}
						break;

					case 9:
						if (parse_ais_9(&ais, &msg_9) == 0)
						{
							userid = msg_9.userid;
							pos2ddd(msg_9.latitude, msg_9.longitude, &lat_dd, &long_ddd);
							ais_entity* entity=find_and_create_ais(&ais_list, userid);
							snprintf(entity->latitude, 24, "%0.6f", lat_dd);
							snprintf(entity->longitude, 24, "%0.6f", long_ddd);
						}
						break;

					case 11:
						if (parse_ais_11(&ais, &msg_11) == 0)
						{
							userid = msg_11.userid;
							pos2ddd(msg_11.latitude, msg_11.longitude, &lat_dd, &long_ddd);
							ais_entity* entity=find_and_create_ais(&ais_list, userid);
							snprintf(entity->latitude, 24, "%0.6f", lat_dd);
							snprintf(entity->longitude, 24, "%0.6f", long_ddd);
							snprintf(entity->date, 24, "%04u-%02u-%02u %02u:%02u:%02u",
							 	msg_4.utc_year, msg_4.utc_month, msg_4.utc_day,
							  	msg_4.utc_hour, msg_4.utc_minute, msg_4.utc_second);				
							}
						break;

					case 18:
						if (parse_ais_18(&ais, &msg_18) == 0)
						{
							userid = msg_18.userid;
							pos2ddd(msg_18.latitude, msg_18.longitude, &lat_dd, &long_ddd);
							ais_entity* entity=find_and_create_ais(&ais_list, userid);
							snprintf(entity->true, 24, "%d", msg_18.true);
							snprintf(entity->latitude, 24, "%0.6f", lat_dd);
							snprintf(entity->longitude, 24, "%0.6f", long_ddd);
						}
						break;

					case 19:
						if (parse_ais_19(&ais, &msg_19) == 0)
						{
							userid = msg_19.userid;
							pos2ddd(msg_19.latitude, msg_19.longitude, &lat_dd, &long_ddd);
							ais_entity* entity=find_and_create_ais(&ais_list, userid);
							snprintf(entity->true, 24, "%d", msg_19.true);
							snprintf(entity->latitude, 24, "%0.6f", lat_dd);
							snprintf(entity->longitude, 24, "%0.6f", long_ddd);
							strncpy(entity->name, 24, msg_5.name);

						}
						break;
					default:
						continue;

					} /* switch msgid */


					

					ais_entity* loop=ais_list;
					end_buffer[0]=0;
					sprintf(end_buffer, "\4{ \"Entities\":[");
					/*snprintf(end_buffer + strlen(end_buffer), OUTPUTBUFFER - strlen(end_buffer), "%s", "ghi");*/

					while(loop) {
						snprintf(end_buffer + strlen(end_buffer), OUTPUTBUFFER - strlen(end_buffer), "{");
						snprintf(end_buffer + strlen(end_buffer), OUTPUTBUFFER - strlen(end_buffer),
							"\"Mmsi\": \"%09ld\", ", userid);
						if (strlen(loop->name)) 
							snprintf(end_buffer + strlen(end_buffer), OUTPUTBUFFER - strlen(end_buffer),
								"\"Name\": \"%s\",", loop->name);
						if (strlen(loop->latitude)) 
							snprintf(end_buffer + strlen(end_buffer), OUTPUTBUFFER - strlen(end_buffer),
								"\"Latitude\": \"%s\",", loop->latitude);
						if (strlen(loop->longitude)) 
							snprintf(end_buffer + strlen(end_buffer), OUTPUTBUFFER - strlen(end_buffer),
								"\"Longitude\": \"%s\",", loop->longitude);
						if (loop->true!=-1000) 
							snprintf(end_buffer + strlen(end_buffer), OUTPUTBUFFER - strlen(end_buffer),
								"\"True\": \"%d\",", loop->true);
						if (strlen(loop->callsign)) 
							snprintf(end_buffer + strlen(end_buffer), OUTPUTBUFFER - strlen(end_buffer),
								"\"Callsign\": \"%s\",", loop->callsign);
						if (strlen(loop->date))
							snprintf(end_buffer + strlen(end_buffer), OUTPUTBUFFER - strlen(end_buffer),
								"\"Utc\": \"%s\",", loop->date);
						if (strlen(loop->dest))
							snprintf(end_buffer + strlen(end_buffer), OUTPUTBUFFER - strlen(end_buffer),
								"\"Destination\": \"%s\",", loop->dest);

						snprintf(end_buffer + strlen(end_buffer), OUTPUTBUFFER - strlen(end_buffer), "},");
						loop=loop->next;
					}
					snprintf(end_buffer + strlen(end_buffer), OUTPUTBUFFER - strlen(end_buffer), "] }");
					sendto(output_socket, (const char *)end_buffer, strlen(end_buffer), 
        				MSG_CONFIRM, (const struct sockaddr *) &servaddr,  
            			sizeof(servaddr)); 
				}
			}
		}
		usleep(50000);
	}
	rtl_ais_cleanup(ctx);
	return 0;
}
