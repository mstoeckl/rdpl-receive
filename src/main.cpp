#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <error.h>
#include <memory.h>
#include <sys/errno.h>
#include <netinet/in.h>

#include <string>

// DL_NAME must end in .gz
const char* DL_NAME = "/home/lvuser/FRCUserProgram-dl.gz";
const char* DC_NAME = "/home/lvuser/FRCUserProgram-dl";
const char* TARGET_NAME = "/home/lvuser/FRCUserProgram";

const int PORT = 1511;

#define DBG(X) printf(std::string("line " + std::to_string(__LINE__) + ": " X + "\n").c_str());

void receiveConnection(int conn) {
	// protocol:
	// Tx: Feedback
	// Rx: Just file data
	int32_t size;
	if (read(conn, &size, sizeof(size)) == -1) {
		DBG("read failure: "+ std::string(strerror(errno)));
		return;
	}
	size = ntohl(size);

	DBG("Size will be: "+ std::to_string(size));

	// Set up gzip decompression

	FILE* f = fopen(DL_NAME, "w");
	if (f == nullptr) {
		DBG("File open failure: "+ std::string(strerror(errno)));
		return;
	}

	constexpr int BFSZ = 65536;
	uint8_t buffer[BFSZ];
	do {
		int rd = size < BFSZ ? size : BFSZ;
		int numRead = read(conn, buffer, rd);
		if (numRead < 0) {
			DBG("Read failure: "+ std::string(strerror(errno)));
			fclose(f);
			return;
		}
		if (fwrite(buffer, 1, numRead, f) < 0) {
			fclose(f);
			DBG("File write failure: "+ std::string(strerror(errno)));
			return;
		}
		DBG(
				"Read down to " + std::to_string(size) + " with jump " + std::to_string(rd));
		size -= numRead;
	} while (size > 0);
	fclose(f);

	DBG("File written");

	// Run the entire control process

	char command[1024];
	int wrote = 0;
	wrote = snprintf(command, 1024, "gzip -d < %s > %s\n", DL_NAME, DC_NAME);
	if (wrote == 1024) {
		DBG("command text too long");
		return;
	}
	if (system(command) < 0) {
		DBG("System gunzip call: "+ std::string(strerror(errno)));
		return;
	}

	if (rename(DC_NAME, TARGET_NAME) < 0) {
		DBG("rename() failure: "+ std::string(strerror(errno)));
		return;
	}

	// create startup script here, the good old
#if 0
	wrote = snprintf(command, 1024, "chmod +x %s\n", TARGET_NAME, TARGET_NAME);
#else
	wrote =
			snprintf(command, 1024,
					". /etc/profile.d/natinst-path.sh && chmod a+x %s && /usr/local/frc/bin/frcKillRobot.sh -t -r",
					TARGET_NAME);
#endif
	if (wrote == 1024) {
		DBG("command text too long");
		return;
	}

	if (system(command) < 0) {
		DBG("system() failure: "+ std::string(strerror(errno)));
		return;
	}
}

int main() {
	// on program failure, close socket, reopen, log error
	system("echo \"Hello World\" > /home/admin/tmp.txt");
	while (true) {
		int sock = socket(AF_INET, SOCK_STREAM, 0);

		if (sock == -1) {
			DBG("SOCK = -1");
		}

		int reuseAddr = 1;
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuseAddr,
				sizeof(reuseAddr)) == -1) {
			DBG(
					"SOCK can't setsockopt to reuse: " + std::string(strerror(errno)));
		}

		int optval = 1;
		if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *) (&optval),
				sizeof(optval))) {
			DBG(
					"SOCK can't setsockopt to enable flushing: " + std::string(strerror(errno)));
		}

		struct timeval timeout;
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout,
				sizeof(struct timeval))) {
			DBG(
					"SOCK can't setsockopt to set a low receive timeout: " + std::string(strerror(errno)));
		}
		if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout,
				sizeof(struct timeval))) {
			DBG(
					"SOCK can't setsockopt to set a low sending timeout: " + std::string(strerror(errno)));
		}

		sockaddr_in address;
		memset(&address, 0, sizeof(address));
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(INADDR_ANY);
		address.sin_port = htons(PORT);

		if (bind(sock, (struct sockaddr *) &address, sizeof(address)) == -1) {
			DBG("SOCK can't bind: " + std::string(strerror(errno)));
		}

		if (listen(sock, 1) == -1) {
			DBG("SOCK can't listen: "+ std::string(strerror(errno)));
		}

		while (true) {
			int conn;
			// block until one connected

			sockaddr_in clientAddress;
			socklen_t clientAddressLen = sizeof(clientAddress);
			if ((conn = accept(sock, (struct sockaddr*) &clientAddress,
					&clientAddressLen)) != -1) {
				receiveConnection(conn);
				close(conn);
			}
		}
	}
}
