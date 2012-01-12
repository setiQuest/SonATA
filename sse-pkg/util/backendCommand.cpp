/*******************************************************************************

 File:    backendCommand.cpp
 Project: OpenSonATA
 Authors: Jon Richards, The SETI Institute. Jan 11, 2012

 Copyright 2011 The SETI Institute

 OpenSonATA is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 OpenSonATA is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with OpenSonATA.  If not, see<http://www.gnu.org/licenses/>.
 
 Implementers of this code are requested to include the caption
 "Licensed through SETI" with a link to setiQuest.org.
 
 For alternate licensing arrangements, please contact
 The SETI Institute at www.seti.org or setiquest.org. 

*******************************************************************************/

/* 
 * Sends s request to the backend server and prints out the reply to standard out.
 */

#include <cstdio>
#include <cstdlib>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ncurses.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <arpa/inet.h>
#include <errno.h>


#define SOCKET_ERROR -1
#define READ_WITH_SELECT_ERROR -1

/**
 * Turns an IP address in character form, 000.000.000.000
 * into the equivalent unsigned int.
 *
 * @param numAsText the IP address as a character.
 * @return the IP address as a number.
 */
unsigned int getIpAddressNum(char *numAsText)
{
    struct in_addr in;
    inet_aton(numAsText, &in);
    return in.s_addr;
}



/**
 * Connnects to the BackendServer.
 *
 * @param ipAddress the address of the controller.
 * @param port the port of the controller.
 * @return the Socket number.
 * @throws exception of there is an error.
 */
int connect2BackendServer(unsigned int ipAddress,
                unsigned short port)
{
        int S=-1;
        struct sockaddr_in A;
        int i = 0;
        int flags = 0;
        fd_set readfds,writefds;
        struct timeval tv;

        //Open and connect to a TCP socket
        S = socket (AF_INET, SOCK_STREAM, 0);

        if(S < 0)
        {
            return SOCKET_ERROR;
        }

        memset(&A,0,sizeof(A));
        A.sin_family = AF_INET;
        A.sin_port = htons (port);
        A.sin_addr.s_addr = ipAddress;

        /* Set to non-blocking. */
        flags = fcntl(S,F_GETFL);
        flags|=O_NONBLOCK;
        fcntl(S,F_SETFL,flags);

        int bufsize = 16384;
        if ((setsockopt(S, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize))) == -1)
        {
          fprintf(stderr, "error setsockopt(SO_RCVBUF)");
          close(S);
          return(SOCKET_ERROR);
        }

        /* Do the connect. */
        errno = 0;
        i = connect (S, (const sockaddr *)&A, sizeof (struct sockaddr_in));

        if((i==-1)&&(errno==EINPROGRESS))
        {
                /* Set up what to watch. */
                FD_ZERO(&readfds);
                FD_SET(S, &readfds);
                FD_ZERO(&writefds);
                FD_SET(S, &writefds);

                /* Wait up to 4 seconds. */
                tv.tv_sec = 0;
                tv.tv_usec = 500000;

                /* Wait. */
                i=select(S+1,NULL,&writefds,NULL,&tv);

                if(i<=0)
                {
                        close(S);
                        return SOCKET_ERROR;
                }
        }
        else if(i == -1)
        {
                close(S);
                fprintf(stderr, "Connect Errno=" + errno);
                return SOCKET_ERROR;
        }

        flags = fcntl(S,F_GETFL);
        flags&=~O_NONBLOCK;
        fcntl(S,F_SETFL,flags);

        return S;

}

/**
 * Read from a file descriptor using select() to wait
 * for data ready to be read.
 *
 * @param fd_in the open file descriptor.
 * @param nMs the max number of milliseconds to wait.
 * @param buf the character buffer containing the read data.
 * @param len the length, in bytes, of buf.
 * @return number of bytes read, or READ_WITH_SELECT_ERROR if
 * select() returned an error.
 */
int readWithSelect (int fd, int nMs, char *buf, int len)
{
        fd_set fds;
        fd_set err_fds;
        struct timeval tv;
        int nResult;

        FD_ZERO (&fds);
        FD_ZERO (&err_fds);
        FD_SET (fd, &fds);
        FD_SET (fd, &err_fds);
      
        memset(&tv, 0, sizeof(tv)); 
        tv.tv_sec = (int)(nMs/1000);
        tv.tv_usec = (nMs*1000 - tv.tv_sec*1000000);

        nResult = select (fd + 1, &fds, NULL, &err_fds, &tv);
    
        if (nResult > 0)
        {
    
                if (FD_ISSET (fd, &fds))
                {
                        //ready for write, do it
                        return recv(fd, buf, len, 0);

                }
                if (FD_ISSET (fd, &err_fds))
                {
                        return (READ_WITH_SELECT_ERROR);

                }
        }
        else
        {
          //fprintf(stderr, "Read error");
        }

        return READ_WITH_SELECT_ERROR;
}


/**
 * Write to a file descriptor using select() to wait
 * for the file descriptor ready to write the data.
 *
 * @param fd_in the open file descriptor.
 * @param nMs the max number of milliseconds to wait.
 * @param buf the character buffer containing the data to write.
 * @param len the length, in bytes, of the data to write.
 * @return number of bytes read, or READ_WITH_SELECT_ERROR if
 * select() returned an error, or SOCKET_ERROR if select()
 * did not return > 0.
 */
int writeWithSelect (int fd, int nMs, const char *buf, int len)
{
        fd_set fds;
        fd_set err_fds;
        struct timeval tv;
        int nResult;

        FD_ZERO (&fds);
        FD_ZERO (&err_fds);
        FD_SET (fd, &fds);
        FD_SET (fd, &err_fds);

        tv.tv_sec = 0;
        tv.tv_usec = nMs*1000;

        nResult = select (fd + 1, NULL, &fds, &err_fds, &tv);

        if (nResult > 0)
        {

                if (FD_ISSET (fd, &fds))
                {
                        //ready for write, do it
                        return write(fd, buf, len);

                }
                if (FD_ISSET (fd, &err_fds))
                {
                        return (READ_WITH_SELECT_ERROR);

                }
        }

        return SOCKET_ERROR;
}

/**
 * Main entry point for the program.
 *
 * @param argc the number of arguments in argv[].
 * @param argv the list of arguments.
 * @return 0 if successful, 1 if error.
 */
int main(int argc, char *argv[])
{
    char buf[1024];
    int fd = -1;
    unsigned int ipAddress = 0;

    memset(buf, 0, sizeof(buf));

    //Get the IP address
    ipAddress = getIpAddressNum((char *)"10.3.0.40");
    if(ipAddress == 0)
    {
      fprintf(stderr, "Error: IP conversion error for 10.3.0.40");
      return 1;
    }

    //Connect to the BackendServer
    fd = connect2BackendServer(ipAddress, 1086);
    if(fd > 0)
    {
      //Construct command
      strcpy(buf, argv[1]);
      for(int i=2; i<argc; i++)
      {
        strcat(buf, " ");
        strcat(buf, argv[i]);
      }

      if(buf[0] == 0)
      {
	      strcat(buf, "-help");
      }
      strcat(buf, "\n");

      //Send the command
      //fprintf(stderr, "Writing %s\n", buf);
      int count = writeWithSelect(fd, 10000, buf, (int)strlen(buf));
      if(count != strlen(buf))
      {
        fprintf(stderr, "Error writing. Exiting.\n");
        exit(1);
      }

      //fprintf(stdout, "Wrote %d bytes.\n", count);

      //Read the result
      memset(buf, 0, sizeof(buf));
      count = readWithSelect(fd, 10000, buf, sizeof(buf));
      if(count > 0)
      {
	fprintf(stdout, "%s", buf);
      }
      else
      {
	fprintf(stderr, "ERROR, nothing returned.");
	exit(0);
      }

      while(count > 0)
      {
        memset(buf, 0, sizeof(buf));
        count = readWithSelect(fd, 1000, buf, sizeof(buf));
        if(count > 0)
        {
  	  fprintf(stdout, "%s", buf);
        }
      }
      close(fd);
    }
    else
    {
      fprintf(stderr,"Error connecting to the BackendServer, 10.3.0.40:1085.\n");
      return 1;
    }

    return 0;
    
}
