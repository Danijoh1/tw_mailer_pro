#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <iostream>

///////////////////////////////////////////////////////////////////////////////

#define BUF 1024
#define PORT 6543

///////////////////////////////////////////////////////////////////////////////

using namespace std;

///////////////////////////////////////////////////////////////////////////////

char* input(char* buffer, int length)
{
   int size;
   printf(">> ");
      if (fgets(buffer, length - 1, stdin) != NULL)
      {
         size = strlen(buffer);
         // remove new-line signs from string at the end
         if (buffer[size - 2] == '\r' && buffer[size - 1] == '\n')
         {
            size -= 2;
            buffer[size] = 0;
         }
         else if (buffer[size - 1] == '\n')
         {
            --size;
            buffer[size] = 0;
         }
      }
   return buffer;
}

int main(int argc, char **argv)
{
   int create_socket;
   char buffer[BUF];
   struct sockaddr_in address;
   int isQuit;
   char commands[5][5] = {"quit","send", "list", "read", "del"};

   ////////////////////////////////////////////////////////////////////////////
   // CREATE A SOCKET
   // https://man7.org/linux/man-pages/man2/socket.2.html
   // https://man7.org/linux/man-pages/man7/ip.7.html
   // https://man7.org/linux/man-pages/man7/tcp.7.html
   // IPv4, TCP (connection oriented), IP (same as server)
   if ((create_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
   {
      perror("Socket error");
      return EXIT_FAILURE;
   }

   ////////////////////////////////////////////////////////////////////////////
   // INIT ADDRESS
   // Attention: network byte order => big endian
   memset(&address, 0, sizeof(address)); // init storage with 0
   address.sin_family = AF_INET;         // IPv4
   // https://man7.org/linux/man-pages/man3/htons.3.html

   // https://man7.org/linux/man-pages/man3/inet_aton.3.html
   if (argc < 3)
   {
      cerr << "Address insufficently defined - connect to default address 127.0.0.1:" << PORT << endl;
      inet_aton("127.0.0.1", &address.sin_addr);
      address.sin_port = htons(PORT);
   }
   else
   {
      inet_aton(argv[1], &address.sin_addr);
      address.sin_port = htons(atoi(argv[2]));

   }

   ////////////////////////////////////////////////////////////////////////////
   // CREATE A CONNECTION
   // https://man7.org/linux/man-pages/man2/connect.2.html
   if (connect(create_socket,
               (struct sockaddr *)&address,
               sizeof(address)) == -1)
   {
      // https://man7.org/linux/man-pages/man3/perror.3.html
      perror("Connect error - no server available");
      return EXIT_FAILURE;
   }

   // ignore return value of printf
   printf("Connection with server (%s) at port (%d) established\n",
          inet_ntoa(address.sin_addr), ntohs(address.sin_port));

   ////////////////////////////////////////////////////////////////////////////
   // RECEIVE DATA
   // https://man7.org/linux/man-pages/man2/recv.2.html
   int size = recv(create_socket, buffer, BUF - 1, 0);
   if (size == -1)
   {
      perror("recv error");
   }
   else if (size == 0)
   {
      printf("Server closed remote socket\n"); // ignore error
   }
   else
   {
      buffer[size] = '\0';
      printf("%s", buffer); // ignore error
   }

   do
   {
      int isValid = 1;
      int command = -1;
      strcpy(buffer,input(buffer, BUF));
      char str[1024] = "";
      strcpy(str, buffer);
      int len =strlen(str);
      for(int i = 0; i < len; i++)
      {
         str[i] = tolower(str[i]);
      }
      isQuit = strcmp(str, "quit") == 0;
      for(int i = 0; i < 5; i++)
      {
         if(isValid != 0)
         {
            isValid = strcmp(str, commands[i]);
            command = i;
         }
      }
      //////////////////////////////////////////////////////////////////////
      // SEND DATA
      // https://man7.org/linux/man-pages/man2/send.2.html
      // send will fail if connection is closed, but does not set
      // the error of send, but still the count of bytes sent
      if(isValid == 0)
      {
         if ((send(create_socket, buffer, size + 1, 0)) == -1) 
         {
            // in case the server is gone offline we will still not enter
            // this part of code: see docs: https://linux.die.net/man/3/send
            // >> Successful completion of a call to send() does not guarantee 
            // >> delivery of the message. A return value of -1 indicates only 
            // >> locally-detected errors.
            // ... but
            // to check the connection before send is sense-less because
            // after checking the communication can fail (so we would need
            // to have 1 atomic operation to check...)
            perror("send error");
            break;
         }
         if(command > 0)
         {
            if(command == 2)
            {
               strcpy(buffer,input(buffer, BUF));
               if ((send(create_socket, buffer, size + 1, 0)) == -1) 
               {
                  perror("send error");
                  break;
               }
            }
            else
            {
               for(int i = 0; i < 2; i++)
               {
                  strcpy(buffer,input(buffer, BUF));
                  if ((send(create_socket, buffer, size + 1, 0)) == -1) 
                  {
                     perror("send error");
                     break;
                  }
               }
            }
            if(command == 1)
            {
               strcpy(buffer,input(buffer, 81));
               if ((send(create_socket, buffer, size + 1, 0)) == -1) 
               {
                  perror("send error");
                  break;
               }
               while(strcmp(buffer, ".") != 0 && strlen(buffer) != 1) 
               {
                  strcpy(buffer,input(buffer, BUF));
                  if ((send(create_socket, buffer, size + 1, 0)) == -1) 
                  {
                     perror("send error");
                     break;
                  }
               }
            }
         }
         //////////////////////////////////////////////////////////////////////
         // RECEIVE FEEDBACK
         // consider: reconnect handling might be appropriate in somes cases
         //           How can we determine that the command sent was received 
         //           or not? 
         //           - Resend, might change state too often. 
         //           - Else a command might have been lost.
         //
         // solution 1: adding meta-data (unique command id) and check on the
         //             server if already processed.
         // solution 2: add an infrastructure component for messaging (broker)
         //
         size = recv(create_socket, buffer, BUF - 1, 0);
         if (size == -1)
         {
            perror("recv error");
            break;
         }
         else if (size == 0)
         {
            if(!isQuit)
            {
               printf("Server closed remote socket\n"); // ignore error
               break;
            }
         }
         else
         {
            buffer[size] = '\0';
            printf("<< %s\n", buffer); // ignore error
            if (strcmp("OK", buffer) != 0)
            {
               fprintf(stderr, "<< Server error occured, abort\n");
               break;
            }
         }
      }
      else
      {
         if(!isQuit)
         {
            fprintf(stderr, "<< Invalid Command\n");
         }
      }
} while (!isQuit);

   ////////////////////////////////////////////////////////////////////////////
   // CLOSES THE DESCRIPTOR
   if (create_socket != -1)
   {
      if (shutdown(create_socket, SHUT_RDWR) == -1)
      {
         // invalid in case the server is gone already
         perror("shutdown create_socket"); 
      }
      if (close(create_socket) == -1)
      {
         perror("close create_socket");
      }
      create_socket = -1;
   }

   return EXIT_SUCCESS;
}
