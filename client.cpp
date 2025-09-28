/*
	Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
	
	Please include your Name, UIN, and the date below
	Name: Fernando Cifuentes
	UIN: 633008180
	Date: 9/26/25
*/
#include "common.h"
#include "FIFORequestChannel.h"
#include <fstream>
#include <unistd.h>
#include <sys/wait.h>

using namespace std;


int main (int argc, char *argv[]) {
	int opt;
	int p = -1;
	double t = -1.0;
	int e = -1;
	int m = MAX_MESSAGE;
	bool new_chan = false;
	vector<FIFORequestChannel*> channels;
	
	string filename = "";
	while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
		switch (opt) {
			case 'p':
				p = atoi (optarg);
				break;
			case 't':
				t = atof (optarg);
				break;
			case 'e':
				e = atoi (optarg);
				break;
			case 'f':
				filename = optarg;
				break;
			case 'm':
				m = atoi(optarg);
				break;
			case 'c':
				new_chan = true;
				break;
		}
	}

	char* server_args[] = { (char*)"./server", (char*)"-m", (char*)to_string(m).c_str() , NULL };
	pid_t pid = fork();

	if (pid == 0) {
		execvp(server_args[0], server_args);
		perror("execvp failed");
		exit(1);
	} else {
		FIFORequestChannel cont_chan("control", FIFORequestChannel::CLIENT_SIDE);
		channels.push_back(&cont_chan);
		
		if(new_chan) {
			MESSAGE_TYPE nc = NEWCHANNEL_MSG;
			cont_chan.cwrite(&nc, sizeof(MESSAGE_TYPE));
			char chan_buf[MAX_MESSAGE];
			cont_chan.cread(&chan_buf, MAX_MESSAGE);
			string chan_name = chan_buf;
			FIFORequestChannel* new_channel = new FIFORequestChannel(chan_name, FIFORequestChannel::CLIENT_SIDE);
			channels.push_back(new_channel);
		}

		FIFORequestChannel chan = *(channels.back());

		if (p != -1 && t != -1.0 && e != -1) {
			char buf[MAX_MESSAGE]; // 256
			datamsg x(p, t, e);
			memcpy(buf, &x, sizeof(datamsg));
			chan.cwrite(&x, sizeof(datamsg)); // question
			double reply;
			chan.cread(&reply, sizeof(double)); //answer
			cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
		} else if (p != -1) {
			ofstream output_file("received/x1.csv");
			for (double i = 0.0; i < 4.0; i += 0.004) {
				datamsg ecg1(p, i, 1);
				datamsg ecg2(p, i, 2);
				double reply1;
				double reply2;
				chan.cwrite(&ecg1, sizeof(datamsg));
				chan.cread(&reply1, sizeof(double));
				chan.cwrite(&ecg2, sizeof(datamsg));
				chan.cread(&reply2, sizeof(double));
				output_file << i << "," << reply1 << "," << reply2 << endl;
			}
		}

		if (!filename.empty()) {
			filemsg fm(0, 0);
			string fname = filename;
			
			int len = sizeof(filemsg) + (fname.size() + 1);
			char* buf2 = new char[len];
			memcpy(buf2, &fm, sizeof(filemsg));
			strcpy(buf2 + sizeof(filemsg), fname.c_str());
			chan.cwrite(buf2, len);  // I want the file length;

			int64_t filesize = 0;
			chan.cread(&filesize, sizeof(int64_t));

			char* buf3 = new char[m];
			filemsg* file_req = (filemsg*)buf2;
			ofstream output_file("received/" + fname);
			int64_t num_chunks = (filesize + m - 1LL) / m;
            for (int64_t i = 0; i < num_chunks; i++) {
				file_req->offset = i * m;
				if (filesize - file_req->offset < m)
					file_req->length = filesize - file_req->offset;
				else
					file_req->length = m;
				chan.cwrite(buf2, len);
				chan.cread(buf3, file_req->length);
				output_file.write(buf3, file_req->length);
			}

			delete[] buf2;
			delete[] buf3;
		}
		// closing the channel    
		MESSAGE_TYPE m = QUIT_MSG;
		chan.cwrite(&m, sizeof(MESSAGE_TYPE));

		if (new_chan) {
			cont_chan.cwrite(&m, sizeof(MESSAGE_TYPE));
            delete channels.back();
        }

		wait(NULL);
	}
}
