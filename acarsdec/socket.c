/*
 *  Copyright (c) 2018 Electrosense
 *
 *   
 *   This code is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License version 2
 *   published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <endian.h>
#include <stdint.h>
#include "acarsdec.h"

struct ESenseHeader {
    uint64_t centerFrequency;
    uint32_t gain;
    uint32_t nSamples;
} __attribute__((packed));

static int fd;

// set the sameple rate by changing RTMULT
// 2.5Ms/s is the best but could be over limit for some hardware
// 2.0Ms/s is safer
#define RTLMULT 160	// 2.0000 Ms/s
//#define RTLMULT 192	// 2.4000 Ms/s
//#define RTLMULT 200   // 2.5000 Ms/s
#define RTLINRATE (INTRATE*RTLMULT)

#define RTLOUTBUFSZ 1024
#define RTLINBUFSZ (RTLOUTBUFSZ*RTLMULT*2)

#define FC 131750000ul

int initSocket(const char * sockName)
{
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
			perror("Open unix socket");
			exit(1);
	}
	struct sockaddr_un addr;
	memset(&addr, 0x0, sizeof addr);
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, sockName, sizeof(addr.sun_path) - 1);
	
	int rc = connect(fd, (struct sockaddr*)&addr, sizeof addr);
	if (rc < 0) {
			perror("Connect unix socket");
			exit(1);
	}
	
	int n;
	unsigned int Fd[MAXNBCHANNELS];

	const float charg[] = { 131.450, 131.525, 131.550, 131.725 };
	for (nbch = 0; nbch < 4; ++nbch) {
		Fd[nbch] = 
		    ((int)(1000000 * charg[nbch] + INTRATE / 2) / INTRATE) *
		    INTRATE;
		if (Fd[nbch] < 118000000 || Fd[nbch] > 138000000) {
			fprintf(stderr, "WARNING: Invalid frequency %d\n",
				Fd[nbch]);
			continue;
		}
		channel[nbch].chn = nbch;
		channel[nbch].Fr = (float)Fd[nbch];
	};
	
	for (n = 0; n < nbch; n++) {
		channel_t *ch = &(channel[n]);
		int ind;
		float AMFreq;

		ch->wf = malloc(RTLMULT * sizeof(float complex));
		ch->dm_buffer=malloc(RTLOUTBUFSZ*sizeof(float));

		AMFreq = (ch->Fr - (float)FC) / (float)(RTLINRATE) * 2.0 * M_PI;
		for (ind = 0; ind < RTLMULT; ind++) {
			ch->wf[ind]=cexpf(AMFreq*ind*-I)/RTLMULT/127.5;
		}
	}

		return 0;
}

static void in_callback(unsigned char *rtlinbuff, uint32_t nread, void *ctx)
{
	int n;

	for (n = 0; n < nbch; n++) {
		channel_t *ch = &(channel[n]);
		int i,m;
		float complex D,*wf;

		wf = ch->wf;
		m=0;
		for (i = 0; i < RTLINBUFSZ;) {
			int ind;

			D = 0;
			for (ind = 0; ind < RTLMULT; ind++) {
				float r, g;
				float complex v;

				r = (float)rtlinbuff[i] - (float)127.37; i++;
				g = (float)rtlinbuff[i] - (float)127.37; i++;

				v=r+g*I;
				D+=v*wf[ind];
			}
			ch->dm_buffer[m++]=cabsf(D);
		}
		demodMSK(ch,m);
	}
}

int runSocket(void)
{
	struct ESenseHeader hdr;
	unsigned char buf[RTLINBUFSZ + 256 * 1024];
	unsigned char * cur = buf;

	while (1) {
		ssize_t nread = recv(fd, &hdr, sizeof hdr, MSG_WAITALL);
		if (nread != sizeof hdr)
				return 1;
		uint64_t centerFrequency = le64toh(hdr.centerFrequency);
		if (centerFrequency != FC) {
			fprintf(stderr, "Unexpected center frequency, bailing out\n");
			return 1;
		}
		uint32_t nSamples = le32toh(hdr.nSamples);
		nread = recv(fd, cur, nSamples, MSG_WAITALL);
		if (nread != nSamples)
				return 1;
		cur += nread;
		while (cur - buf >= RTLINBUFSZ) {
			in_callback(buf, RTLINBUFSZ, NULL);
			memmove(buf, buf + RTLINBUFSZ, cur - (buf + RTLINBUFSZ));
			cur -= RTLINBUFSZ;
		}
	}
}

