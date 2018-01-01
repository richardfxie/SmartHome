/* 
 * usage: thermostat <port>
 * 
 * Richard Xie
 *
 * To compile: gcc -Wall -O2 -o thermostat thermostat.c -lpthread -lwiringPi
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <dirent.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <wiringPi.h>

#define BUFSIZE 4096
#define MAXLINE 4096

typedef struct {
    char cmd[5];
    char mode[5];
    char request[65];
    char data[65];
} http_request;
typedef struct {
    float room_temperature;
    char response[1024];
} http_response;

typedef struct {
   int thread_id;
   float target_temp;
} thdata;
thdata data = {1, 23.0f};


int mode=0; //0: off, 3: auto, 2: cool, 1: heat
float target_temp = 23.0f;
const float delta = 0.25f;
const int acPin = 17;
const int heatPin = 27;
const int fanPin = 22;

int off_duration=30;
int isOn = 0;
time_t last_off;

char ds18b20_path[80];

/*
 *  populates the path of DS18B20, which has a file that shows the current 
 *  temperature.
 */
void get_ds18b20_path()
{
    memset(ds18b20_path, 0, 80);
    DIR *d;
    struct dirent *dir;
    char device_name[80];
    memset(device_name, 0, 80);

    d = opendir("/sys/bus/w1/devices");
    if (d) {
        while ((dir = readdir(d)) != NULL)
        {
                strcpy(device_name, dir->d_name);
                //printf("%s\n", device_name);
        }
        closedir(d);
        strcpy(ds18b20_path, "/sys/bus/w1/devices/");
        strcat(ds18b20_path, device_name);
        strcat(ds18b20_path, "/w1_slave");


       // printf("%s\n", ds18b20_path);
    }
}
/* return temperature in Celcius measured by DS18B20 */
int get_temperature()
{
   char buf[1025];

   memset(buf, 0, 1025);
   FILE *fp = fopen(ds18b20_path, "r");
   if(fp != NULL)
   {
      size_t len = fread(buf, sizeof(char), 1025, fp);
       fclose(fp);
   }
   //printf("w1_slave=[%s]\n", buf);
   //printf("target_temp=%d\n", data.target_temp);
   int len = strlen(buf);

   int i;

   int index;
   for (i = len -1; i>=0 && buf[i] != '='; i--) {
        index = i;
   }

   char temperature[16];

   memset(temperature, 0, 16);

   strncpy(temperature, buf + index, len - index);

   //printf("i = %d temperature=%s\n", index, temperature);
   int t = atoi(temperature);
   //printf("target_temp=%f\n", target_temp);
   return t;
}

/* 
 * This function parse the request of clients.
 */
void parse_request(char *buf, http_request *req, http_response *res){
    char method[MAXLINE], uri[MAXLINE];
   

 //printf("buf=[%s] temp=%f\n", buf, data.target_temp);
    char cmd[4];
    memset(cmd, 0, 4);
    strncpy(cmd, buf, 3);
    if (strcmp(cmd, "GET") == 0) {
	int index;
    	int j;
	for (j  = 5; j< strlen(buf) && buf[j] != ' ';j++) {
		index = j;
	} 
	char uri[32];
        memset(uri, 0, 32);
	strncpy(uri, buf+5, index - 4);
        //printf("uri=[%s]\n", uri);
	if (strcmp(uri, "temperature") == 0) {
		int room_temp = get_temperature();
		res->room_temperature = room_temp * 0.001;
		sprintf(res->response, "{\"data\":{\"temperature\":\"%.3f\"}}\n", res->room_temperature );
	}
	else if (strcmp(uri, "mode") == 0) {
		
		sprintf(res->response, "{\"data\":{\"mode\":\"%d\"}}\n", mode );
	}
	else if (strcmp(uri, "target_temperature") == 0) {
		
		sprintf(res->response, "{\"data\":{\"target_temperature\":\"%.3f\"}}\n", target_temp );
	}
    }
    else if (strcmp(cmd, "PUT") == 0) {
	int len = strlen(buf);
        int i, temp_begin, equal_begin;
        for (i = len - 1; i>=0; i--) {

		if (buf[i] == '=') {
			equal_begin = i;
		}
		else if (buf[i] == '\n') {
			temp_begin = i;
			break;
		}
	}

//printf(".................[%d][%d]2\n", temp_begin, equal_begin);
	char put_cmd[32];
	char target_temperature[16];

	memset(put_cmd, 0, 32);
	memset(target_temperature, 0, 16);

        strncpy(put_cmd, buf+temp_begin+1, equal_begin - temp_begin - 1);
        strncpy(target_temperature, buf+equal_begin + 1, len - equal_begin - 1);
	//printf("command=%s \n", put_cmd);
	//printf("temp=%s \n", target_temperature);
	if (strcmp("temperature", put_cmd) == 0)
	{
		if (atof(target_temperature) > 0.0) 
		{
			target_temp = atof(target_temperature);
			data.target_temp = target_temp;
			sprintf(res->response, "{\"data\":{\"result\";\"temperature set to %.3f degree\"}}", target_temp);
		}
		else {
			sprintf(res->response, "{\"data\":{\"result\";\"temperature set failed\"}}" );

		}
	}
	else 
	if (strcmp("mode", put_cmd) == 0)
	{
		int req_mode = atoi(target_temperature);

		if (req_mode == 0) {
			mode = req_mode;
			sprintf(res->response, "{\"data\":{\"result\";\"mode set to off\"}}\n" );
		}
		else
		if (req_mode == 3) {
			mode = req_mode;
			sprintf(res->response, "{\"data\":{\"result\";\"mode set to auto\"}}\n" );
		}
		else
		if (req_mode == 2) {
			mode = req_mode;
			sprintf(res->response, "{\"data\":{\"result\";\"mode set to cool\"}}\n" );
		}
		else
		if (req_mode == 1) {
			mode = req_mode;
			sprintf(res->response, "{\"data\":{\"result\";\"mode set to heat\"}}\n" );
		}
		else {
			sprintf(res->response, "{\"data\":{\"result\";\"mode not changed\"}}\n" );
		}
		
	}

    }

}

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

/*
 * This method controls the way thermostat works. 
 * if mode is set to heat and room temperature is lower than the set temperature * then the GPIO pin for heat is set to high, which turns on photocoupler, which * turns on the triac and therefore turns on heater. Likewise, the way AC is  
 * controlled in similar way.
 */
void control_thermostat(void *ptr)
{
  //thdata *data = (thdata *) ptr;
   while (1) {
	// data.target_temp += 3.0f;
	//printf("temp=%f\n", data.target_temp);
	if (mode == 0) {
		digitalWrite(acPin, LOW);
		digitalWrite(heatPin, LOW);
		digitalWrite(fanPin, LOW);
		if (isOn != 0)
		    last_off = time(NULL);
		isOn = 0;
	} else if (mode == 3) {
		time_t now = time(NULL);
		if (now - last_off > off_duration) {
			digitalWrite(acPin, LOW);
			digitalWrite(heatPin, LOW);
			digitalWrite(fanPin, HIGH);
			isOn = 1;
		}
	} else
	if (mode == 2) { // AC mode
             int room_temp = get_temperature();
             float room_temperature = room_temp * 0.001;
	     if (room_temperature > target_temp + delta) {
		//turn on AC
		time_t now = time(NULL);
		if (now - last_off > off_duration) {
			digitalWrite(fanPin, LOW);
			digitalWrite(acPin, HIGH);
			digitalWrite(heatPin, LOW);
			isOn = 1;
		}
	     }
	     else if (room_temperature < target_temp - delta) {
		//turn off AC
		digitalWrite(acPin, LOW);
		digitalWrite(heatPin, LOW);
		digitalWrite(fanPin, LOW);
		if (isOn != 0)
		    last_off = time(NULL);
                isOn = 0;
	     }

	}
	else if (mode == 1) { // Heater mode
             int room_temp = get_temperature();
             float room_temperature = room_temp * 0.001;
	     if (room_temperature < target_temp + delta) {
		//turn on Heater
		time_t now = time(NULL);
		if (now - last_off > off_duration) {
			digitalWrite(fanPin, LOW);
			digitalWrite(heatPin, HIGH);
			digitalWrite(acPin, LOW);
			isOn = 1;
		}
	     }
	     else if (room_temperature > target_temp + delta) {
		//turn off Heater
		digitalWrite(acPin, LOW);
		digitalWrite(heatPin, LOW);
		digitalWrite(fanPin, LOW);
		if (isOn != 0)
		   last_off = time(NULL);
                isOn = 0;
	     }
	}

	delay(5000); 
   }
}

int main(int argc, char **argv) {
  int parentfd; 	/* parent socket */
  int childfd; 		/* child socket */
  int portno; 		/* port to listen on */
  int clientlen; 	/* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  char buf[BUFSIZE]; 	/* message buffer */
  int optval; 		/* flag value for setsockopt */
  int n; 		/* message byte size */
  int notdone;
  pthread_t thread1;

  fd_set readfds;

  pid_t pid, sid;

  /* 
   * check command line arguments 
   */
  if (argc == 1)
  {
     portno = 9999;
  }
  else
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  else {
     portno = atoi(argv[1]);
  }

  last_off = time(NULL);
  // wiringPiSetup () ;
  wiringPiSetupGpio();
  pinMode(acPin, OUTPUT);
  pinMode(heatPin, OUTPUT);
  pinMode(fanPin, OUTPUT);

  digitalWrite(acPin, LOW);
  digitalWrite(heatPin, LOW);
  digitalWrite(fanPin, LOW);

/* fork a new process so it creates a daemon that runs forever. */

  pid = fork();
  if (pid < 0) {
	exit(EXIT_FAILURE);
  }
  if (pid > 0) {

	exit(EXIT_SUCCESS);
  }
  umask(0);

  sid = setsid();
  if (sid < 0) {
	exit(EXIT_FAILURE);
  }
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);


  get_ds18b20_path();
  pthread_create(&thread1, NULL, (void *) &control_thermostat, (void *) &data);
  /* 
   * socket: create the parent socket 
   */
  parentfd = socket(AF_INET, SOCK_STREAM, 0);
  if (parentfd < 0) 
    error("ERROR opening socket");

  optval = 1;
  setsockopt(parentfd, SOL_SOCKET, SO_REUSEADDR, 
	     (const void *)&optval , sizeof(int));

  /*
   * build the server's Internet address
   */
  bzero((char *) &serveraddr, sizeof(serveraddr));

  /* this is an Internet address */
  serveraddr.sin_family = AF_INET;

  /* let the system figure out our IP address */
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

  /* this is the port we will listen on */
  serveraddr.sin_port = htons((unsigned short)portno);

  /* 
   * bind: associate the parent socket with a port 
   */
  if (bind(parentfd, (struct sockaddr *) &serveraddr, 
	   sizeof(serveraddr)) < 0) 
    error("ERROR on binding");

  /* 
   * listen: make this socket ready to accept connection requests 
   */
  if (listen(parentfd, 5) < 0) /* allow 5 requests to queue up */ 
    error("ERROR on listen");


  /* initialize some things for the main loop */
  clientlen = sizeof(clientaddr);
  notdone = 1;

  /* 
   * main loop: wait for connection request or stdin command.
   *
   * If connection request, then echo input line 
   * and close connection. 
   * If command, then process command.
   */
  while (notdone) {

    /* 
     * select: Has the user typed something to stdin or 
     * has a connection request arrived?
     */
    FD_ZERO(&readfds);          /* initialize the fd set */
    FD_SET(parentfd, &readfds); /* add socket fd */
    //FD_SET(0, &readfds);        /* add stdin fd (0) */
    if (select(parentfd+1, &readfds, 0, 0, 0) < 0) {
      error("ERROR in select");
    }


    /* if a connection request has arrived, process it */
    if (FD_ISSET(parentfd, &readfds)) {
      /* 
       * accept: wait for a connection request 
       */
      childfd = accept(parentfd, 
		       (struct sockaddr *) &clientaddr, &clientlen);
      if (childfd < 0) 
	error("ERROR on accept");
      
      /* 
       * read: read input string from the client
       */
      bzero(buf, BUFSIZE);
      n = read(childfd, buf, BUFSIZE);
      if (n < 0) 
	error("ERROR reading from socket");
      
      http_request req;
      http_response res;
      parse_request(buf, &req, &res);
      char res_buf[BUFSIZE];
      memset(res_buf, 0, BUFSIZE);
      sprintf(res_buf, "HTTP/1.1 200 \r\nConnection: Keep-Alive\r\n Accept: multipart/form-data\r\nAccept-Encoding: multipart/form-data\r\nServer: Simple Rest Server\r\nache-control: no-store\r\nContent-Length: %d\r\nContent-Type: Application\/json\r\n\r\n%s", strlen(res.response), res.response );
      //n = write(childfd, res.response, strlen(res.response));
//printf("response=%s\n", res.response);
      n = write(childfd, res_buf, strlen(res_buf));

      if (n < 0) 
	error("ERROR writing to socket");
      
      close(childfd);
    }
  }

  /* clean up */
  //printf("Terminating server.\n");
  close(parentfd);
  exit(0);
}
