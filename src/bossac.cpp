///////////////////////////////////////////////////////////////////////////////
// BOSSA
//
// Copyright (c) 2011-2012, ShumaTech
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the <organization> nor the
//       names of its contributors may be used to endorse or promote products
//       derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
///////////////////////////////////////////////////////////////////////////////
#include <string>
#include <exception>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <pthread.h>

#define MAX 80
#define PORT 8080
#define SA struct sockaddr

#include "CmdOpts.h"
#include "Samba.h"
#include "PortFactory.h"
#include "FlashFactory.h"
#include "Flasher.h"
#include "SocketPort.h"

void* flasher_thread(void *arg);

using namespace std;

class BossaConfig
{
public:
    BossaConfig();
    virtual ~BossaConfig() {}

    bool erase;
    bool write;
    bool read;
    bool verify;
    bool reset;
    bool port;
    bool boot;
    bool bor;
    bool bod;
    bool lock;
    bool unlock;
    bool security;
    bool info;
    bool debug;
    bool help;
    bool forceUsb;
    bool socket;
    string forceUsbArg;

    int readArg;
    string portArg;
    int bootArg;
    int bodArg;
    int borArg;
    int socketPort;
    string lockArg;
    string unlockArg;
};

BossaConfig::BossaConfig()
{
    erase = false;
    write = false;
    read = false;
    verify = false;
    port = false;
    boot = false;
    bod = false;
    bor = false;
    lock = false;
    security = false;
    info = false;
    help = false;
    forceUsb = false;

    readArg = 0;
    bootArg = 1;
    bodArg = 1;
    borArg = 1;
    socketPort = 4445;

    reset = false;
}

static BossaConfig config;
static Option opts[] =
{
    {
      'e', "erase", &config.erase,
      { ArgNone },
      "erase the entire flash (keep the 8KB of bootloader for SAM Dxx)"
    },
    {
      'w', "write", &config.write,
      { ArgNone },
      "write FILE to the flash; accelerated when\n"
      "combined with erase option"
    },
    {
      'r', "read", &config.read,
      { ArgOptional, ArgInt, "SIZE", { &config.readArg } },
      "read SIZE from flash and store in FILE;\n"
      "read entire flash if SIZE not specified"
    },
    {
      'v', "verify", &config.verify,
      { ArgNone },
      "verify FILE matches flash contents"
    },
    {
      'p', "port", &config.port,
      { ArgRequired, ArgString, "PORT", { &config.portArg } },
      "use serial PORT to communicate to device;\n"
      "default behavior is to auto-scan all serial ports"
    },
    {
      'b', "boot", &config.boot,
      { ArgOptional, ArgInt, "BOOL", { &config.bootArg } },
      "boot from ROM if BOOL is 0;\n"
      "boot from FLASH if BOOL is 1 [default];\n"
      "option is ignored on unsupported devices"
    },
    {
      'c', "bod", &config.bod,
      { ArgOptional, ArgInt, "BOOL", { &config.bodArg } },
      "no brownout detection if BOOL is 0;\n"
      "brownout detection is on if BOOL is 1 [default]"
    },
    {
      't', "bor", &config.bor,
      { ArgOptional, ArgInt, "BOOL", { &config.borArg } },
      "no brownout reset if BOOL is 0;\n"
      "brownout reset is on if BOOL is 1 [default]"
    },
    {
      'l', "lock", &config.lock,
      { ArgOptional, ArgString, "REGION", { &config.lockArg } },
      "lock the flash REGION as a comma-separated list;\n"
      "lock all if not given [default]"
    },
    {
      'u', "unlock", &config.unlock,
      { ArgOptional, ArgString, "REGION", { &config.unlockArg } },
      "unlock the flash REGION as a comma-separated list;\n"
      "unlock all if not given [default]"
    },
    {
      's', "security", &config.security,
      { ArgNone },
      "set the flash security flag"
    },
    {
      'i', "info", &config.info,
      { ArgNone },
      "display device information"
    },
    {
      'd', "debug", &config.debug,
      { ArgNone },
      "print debug messages"
    },
    {
      'h', "help", &config.help,
      { ArgNone },
      "display this help text"
    },
    {
      'U', "force_usb_port", &config.forceUsb,
      { ArgRequired, ArgString, "true/false", { &config.forceUsbArg } },
      "override USB port autodetection"
    },
    {
      'R', "reset", &config.reset,
      { ArgNone },
      "reset CPU (if supported)"
    },
	{
		'S', "socket", &config.socket,
		{ ArgOptional, ArgInt, "PORT", {&config.socketPort}},
		"Start Bossac Server on Port(Default 4445)"
	}
};

bool
autoScan(Samba& samba, PortFactory& portFactory, string& port)
{
    for (port = portFactory.begin();
         port != portFactory.end();
         port = portFactory.next())
    {
        if (config.debug)
            printf("Trying to connect on %s\n", port.c_str());
        if (samba.connect(portFactory.create(port)))
            return true;
    }

    return false;
}

int
help(const char* program)
{
    fprintf(stderr, "Try '%s -h' or '%s --help' for more information\n", program, program);
    return 1;
}

static struct timeval start_time;

void
timer_start()
{
    gettimeofday(&start_time, NULL);
}

float
timer_stop()
{
    struct timeval end;
    gettimeofday(&end, NULL);
    return (end.tv_sec - start_time.tv_sec) + (end.tv_usec - start_time.tv_usec) / 1000000.0;
}
char filename[512];
int
main(int argc, char* argv[])
{
    int args;
    char* pos;
    CmdOpts cmd(argc, argv, sizeof(opts) / sizeof(opts[0]), opts);

    if ((pos = strrchr(argv[0], '/')) || (pos = strrchr(argv[0], '\\')))
        argv[0] = pos + 1;

    if (argc <= 1)
    {
        fprintf(stderr, "%s: you must specify at least one option\n", argv[0]);
        return help(argv[0]);
    }

    args = cmd.parse();
    if (args < 0)
        return help(argv[0]);

    if (config.read && (config.write || config.verify))
    {
        fprintf(stderr, "%s: read option is exclusive of write or verify\n", argv[0]);
        return help(argv[0]);
    }

    if (config.read || config.write || config.verify)
    {
        if (args == argc)
        {
            fprintf(stderr, "%s: missing file\n", argv[0]);
            return help(argv[0]);
        }
        argc--;
    }
    if (args != argc)
    {
        fprintf(stderr, "%s: extra arguments found\n", argv[0]);
        return help(argv[0]);
    }
    strcpy(filename, argv[args]);

    if (config.help)
    {
        printf("Usage: %s [OPTION...] [FILE]\n", argv[0]);
        printf("Basic Open Source SAM-BA Application (BOSSA) Version " VERSION "\n"
               "Flash programmer for Atmel SAM devices.\n"
	       "Copyright (c) 2011-2012 ShumaTech (http://www.shumatech.com)\n"
	       "This build maintained by MattairTech (http://www.mattairtech.com/)\n"
               "\n"
               "Examples:\n"
               "  bossac -e -w -v -b image.bin   # Erase flash, write flash with image.bin,\n"
               "                                 # verify the write, and set boot from flash\n"
               "  bossac -r0x10000 image.bin     # Read 64KB from flash and store in image.bin\n"
              );
        printf("\nOptions:\n");
	cmd.usage(stdout);
	printf("\nReport bugs to <support@mattairtech.com>\n");
        printf("\nReport bugs to <bugs@shumatech.com>\n");
        return 1;
    }

    try
    {
        Samba samba;
        PortFactory portFactory;
        FlashFactory flashFactory;
        int opt = 1;
        pthread_t thread_id;

        if (config.debug)
            samba.setDebug(true);

        if(config.socket)
        {

			int sockfd, connfd, len;
			struct sockaddr_in servaddr, cli;

			// socket create and verification
			sockfd = socket(AF_INET, SOCK_STREAM, 0);
			if (sockfd == -1) {
				printf("socket creation failed...\n");
				exit(0);
			}
			else
				printf("Socket successfully created..\n");
			bzero(&servaddr, sizeof(servaddr));

			//set master socket to allow multiple connections ,
		    //this is just a good habit, it will work without this
		    if( setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt,
		          sizeof(opt)) < 0 )
		    {
		        perror("setsockopt");
		        exit(EXIT_FAILURE);
		    }
			// assign IP, PORT
			servaddr.sin_family = AF_INET;
			servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
			servaddr.sin_port = htons(config.socketPort);

			// Binding newly created socket to given IP and verification
			if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
				printf("socket bind failed...\n");
				exit(0);
			}
			else
				printf("Socket successfully binded..\n");

			// Now server is ready to listen and verification
			if ((listen(sockfd, 1)) != 0) {
				printf("Listen failed...\n");
				exit(0);
			}
			else
				printf("Server listening..\n");
			len = sizeof(cli);
			while(1)
			{
				// Accept the data packet from client and verification
				connfd = accept(sockfd, (SA*)&cli, (socklen_t *)&len);
				if (connfd < 0) {
					printf("server acccept failed...\n");
					exit(0);
				}
				else
					printf("server acccept the client...:%d\n", connfd);
			    pthread_create(&thread_id, NULL, flasher_thread, &connfd);
        	}
        }
    }
    catch (exception& e)
    {
        fprintf(stderr, "\n%s\n", e.what());
        return 1;
    }
    catch(...)
    {
        fprintf(stderr, "\nUnhandled exception\n");
        return 1;
    }

    return 0;
}

void* flasher_thread(void *arg)
{
    bool isUsb = true;
    int client_fd = *(int *)arg;

	fprintf(stderr, "Thread Started:%d\n", client_fd);
    try
    {
        Samba samba;
        PortFactory portFactory;
        FlashFactory flashFactory;

		if (config.forceUsb)
		{
			if (config.forceUsbArg.compare("true")==0)
				isUsb = true;
			else if (config.forceUsbArg.compare("false")==0)
				isUsb = false;
			else
			{
				fprintf(stderr, "Invalid USB value: %s\n", config.forceUsbArg.c_str());
				return NULL;
			}
		}

		if (config.port)
		{
			bool res;
			uint8_t ret;

			if (config.forceUsb)
				ret = samba.reboot(portFactory.create(config.portArg, isUsb));
			else
				ret = samba.reboot(portFactory.create(config.portArg));
			if ((ret == 2) || (ret == 3))
			{
				fprintf(stderr, "No device found on %s\n", config.portArg.c_str());
				return NULL;
			}
			else if((ret == 1) || (ret == 0))
			{
				fprintf(stderr, "waiting for device to reboot on %s\n", config.portArg.c_str());
			}


			if (config.forceUsb)
				res = samba.connect(portFactory.create(config.portArg, isUsb));
			else
				res = samba.connect(portFactory.create(config.portArg));
			if (!res)
			{
				fprintf(stderr, "waiting for the device to boot on %s\n", config.portArg.c_str());
				return NULL;
			}
		}
		else
		{
			bool res;
			fprintf(stderr, "Samba Connect start\n");
		    SocketPort *p = new SocketPort(std::string(), client_fd);
			res = samba.connect(SerialPort::Ptr(p));
			if (!res)
			{
				fprintf(stderr, "No device found on\n");
				return NULL;
			}
			fprintf(stderr, "Samba Connected\n");
		}

		fprintf(stderr, "ChipID\n");
		uint32_t chipId = samba.chipId();

		fprintf( stderr, "Atmel SMART device 0x%08x found\n", chipId );
		Flash::Ptr flash = flashFactory.create(samba, chipId);
		if (flash.get() == NULL)
		{
			fprintf(stderr, "Flash for chip ID %08x is not supported\n", chipId);
			return NULL;
		}
		fprintf( stderr, "Atmel SMART device 0x%08x found\n", chipId );
		Flasher flasher(flash);

		if (config.info)
			flasher.info(samba);

		if (config.unlock)
			flasher.lock(config.unlockArg, false);

		if (config.erase)
		{
			timer_start();
			flasher.erase();
			printf("done in %5.3f seconds\n", timer_stop());
		}

		if (config.write)
		{
			printf("\n");
			timer_start();
			flasher.write(filename);
			printf("done in %5.3f seconds\n", timer_stop());
		}

		if (config.verify)
		{
			printf("\n");
			timer_start();
			if (!flasher.verify(filename))
			{
				return NULL;
			}
			printf("done in %5.3f seconds\n", timer_stop());
		}

		if (config.read)
		{
			printf("\n");
			timer_start();
			flasher.read(filename, config.readArg);
			printf("done in %5.3f seconds\n", timer_stop());
		}

		if (config.boot)
		{
			printf("Set boot flash %s\n", config.bootArg ? "true" : "false");
			flash->setBootFlash(config.bootArg);
		}

		if (config.bod)
		{
			printf("Set brownout detect %s\n", config.bodArg ? "true" : "false");
			flash->setBod(config.bodArg);
		}

		if (config.bor)
		{
			printf("Set brownout reset %s\n", config.borArg ? "true" : "false");
			flash->setBor(config.borArg);
		}

		if (config.security)
		{
			printf("Set security\n");
			flash->setSecurity();
		}

		if (config.lock)
			flasher.lock(config.lockArg, true);

		if (config.reset)
			samba.reset();
	}
	catch (exception& e)
	{
		fprintf(stderr, "\n%s\n", e.what());
		return NULL;
	}
	catch(...)
	{
		fprintf(stderr, "\nUnhandled exception\n");
		return NULL;
	}
	return NULL;
}
