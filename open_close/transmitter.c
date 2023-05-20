#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS10"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

volatile int STOP=FALSE;

int ctrl_frame( char a, char c, int f) {
	char buf[5];
	
	buf[0] = 0x5c;
	buf[1] = a;
	buf[2] = c;
	buf[3] = a^c;
	buf[4] = 0x5c;
	
	int res = write(f,buf,5);
	//printf("%d bytes written\n", res);
	return res;
}

int i_frame( char a, char c, int f) {
	char buf[255];
	int i;
	buf[0] = 0x5c;
	buf[1] = a;
	buf[2] = c;
	buf[3] = a^c;
	for (i = 4; i < 253; i++) {
		buf[i] = getchar();
		if ( buf[i] == '\n') break;
	}
	if (buf[4]=='\n') return -1;
	buf[i] = a^c;					//IMPLEMENTAR BCC!!!!!!!!!!!!!!!!
	buf[i+1] = 0x5c;
	
	int res = write(f,buf,i+2);
	//printf("%d bytes written\n", res);
	return res;
}

int check_received( char* mess, char ctrl) {	// MAQUINA DE ESTADOS (verifica se a mensagem foi passada corretamente)

	if (mess[0] != 0x5c) {					//flag1
	    return -1;
	} else if (mess[4] != 0x5c) {				//flag2
	    return -1;	
	} else if (mess[2] != ctrl) {				//ctrl
	    return -1;			
	} else if ( (mess[1]^mess[2]) != mess[3]) {		//bcc
	    return -1;
	} else return 0;
}

int main(int argc, char** argv)
{
    int fd,c, res;
    struct termios oldtio,newtio;
    char buf[255];
    int i, sum = 0, speed = 0;

    if ( (argc < 2) ||
         ((strcmp("/dev/ttyS0", argv[1])!=0) &&
          (strcmp("/dev/ttyS1", argv[1])!=0) )) {
        printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS11\n");
        exit(1);
    }


    /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
    */


    fd = open(argv[1], O_RDWR | O_NOCTTY );
    if (fd < 0) { perror(argv[1]); exit(-1); }

    if ( tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
        perror("tcgetattr");
        exit(-1);
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 1;   /* blocking read until 1 chars received */



    /*
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a
    leitura do(s) próximo(s) caracter(es)
    */


    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd,TCSANOW,&newtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

	char addr	= 0x01;
	char i_f	= 0x02;
	char set	= 0x03;
	char ua		= 0x07;
	char disc 	= 0x0b;
	char rr 	= 0x21;
	char rej 	= 0x25;

    while (STOP==FALSE) {  

	    //OPEN
	    res = ctrl_frame( addr, set, fd);			
	    printf("Set sent\nWaiting for response...\n");
	    res = read(fd,buf,255);   /* returns after 5 chars have been input */
	    if (check_received(buf, ua) != 0 ) break;
	    printf("UA Received\nConection opened\n\n");
	    
	    //WRITE
	    while(TRUE) {
		    
		    if ((res = i_frame( addr, i_f, fd)) == -1) break;
		    printf("%d bytes written\n Waiting for response...\n\n", res);	 
		    
		    res = read(fd,buf,255);   /* returns after 5 chars have been input */
		    if (check_received(buf, rr) != 0 ) {
		    	printf("UA ERROR\n");
		    	break;
		    }
		    printf("RR received\n");   
	    }
	    
	    //CLOSE
	    res = ctrl_frame( addr, disc, fd);			
	    printf("DISC sent\nWaiting for response...\n");
	    res = read(fd,buf,255);   /* returns after 5 chars have been input */
	    if (check_received(buf, disc) != 0 ) break;
	    printf("DISC Received\nSending UA\n");
	    res = ctrl_frame( addr, ua, fd);
	    printf("UA sent\nClosing...\n");
	    
	    STOP=TRUE;
	    memset(buf,0,strlen(buf));
    }
	    
    sleep(1);
    
    if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
	perror("tcsetattr");
	exit(-1);
    }
    
	
    close(fd);
    return 0;
}

//int ctrl_frame(char a, char c, int f ) {


