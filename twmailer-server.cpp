#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <stdexcept>
#include <filesystem> 
#include <fstream> 
#include <iostream>

///////////////////////////////////////////////////////////////////////////////

#define BUF 1024
#define PORT 6543

///////////////////////////////////////////////////////////////////////////////

using namespace std;
using namespace std::filesystem;

///////////////////////////////////////////////////////////////////////////////

int abortRequested = 0;
int create_socket = -1;
int new_socket = -1;

///////////////////////////////////////////////////////////////////////////////

void *clientCommunication(void *data);
void signalHandler(int sig);

///////////////////////////////////////////////////////////////////////////////

int main(void)
{
   socklen_t addrlen;
   struct sockaddr_in address, cliaddress;
   int reuseValue = 1;

   ////////////////////////////////////////////////////////////////////////////
   // SIGNAL HANDLER
   // SIGINT (Interrup: ctrl+c)
   // https://man7.org/linux/man-pages/man2/signal.2.html
   if (signal(SIGINT, signalHandler) == SIG_ERR)
   {
      perror("signal can not be registered");
      return EXIT_FAILURE;
   }

   ////////////////////////////////////////////////////////////////////////////
   // CREATE A SOCKET
   // https://man7.org/linux/man-pages/man2/socket.2.html
   // https://man7.org/linux/man-pages/man7/ip.7.html
   // https://man7.org/linux/man-pages/man7/tcp.7.html
   // IPv4, TCP (connection oriented), IP (same as client)
   if ((create_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
   {
      perror("Socket error"); // errno set by socket()
      return EXIT_FAILURE;
   }

   ////////////////////////////////////////////////////////////////////////////
   // SET SOCKET OPTIONS
   // https://man7.org/linux/man-pages/man2/setsockopt.2.html
   // https://man7.org/linux/man-pages/man7/socket.7.html
   // socket, level, optname, optvalue, optlen
   if (setsockopt(create_socket,
                  SOL_SOCKET,
                  SO_REUSEADDR,
                  &reuseValue,
                  sizeof(reuseValue)) == -1)
   {
      perror("set socket options - reuseAddr");
      return EXIT_FAILURE;
   }

   /*if (setsockopt(create_socket,
                  SOL_SOCKET,
                  SO_REUSEPORT,
                  &reuseValue,
                  sizeof(reuseValue)) == -1)
   {
      perror("set socket options - reusePort");
      return EXIT_FAILURE;
   }*/

   ////////////////////////////////////////////////////////////////////////////
   // INIT ADDRESS
   // Attention: network byte order => big endian
   memset(&address, 0, sizeof(address));
   address.sin_family = AF_INET;
   address.sin_addr.s_addr = INADDR_ANY;
   address.sin_port = htons(PORT);

   ////////////////////////////////////////////////////////////////////////////
   // ASSIGN AN ADDRESS WITH PORT TO SOCKET
   if (bind(create_socket, (struct sockaddr *)&address, sizeof(address)) == -1)
   {
      perror("bind error");
      return EXIT_FAILURE;
   }

   ////////////////////////////////////////////////////////////////////////////
   // ALLOW CONNECTION ESTABLISHING
   // Socket, Backlog (= count of waiting connections allowed)
   if (listen(create_socket, 5) == -1)
   {
      perror("listen error");
      return EXIT_FAILURE;
   }

   while (!abortRequested)
   {
      /////////////////////////////////////////////////////////////////////////
      // ignore errors here... because only information message
      // https://linux.die.net/man/3/printf
      printf("Waiting for connections...\n");

      /////////////////////////////////////////////////////////////////////////
      // ACCEPTS CONNECTION SETUP
      // blocking, might have an accept-error on ctrl+c
      addrlen = sizeof(struct sockaddr_in);
      if ((new_socket = accept(create_socket,
                               (struct sockaddr *)&cliaddress,
                               &addrlen)) == -1)
      {
         if (abortRequested)
         {
            perror("accept error after aborted");
         }
         else
         {
            perror("accept error");
         }
         break;
      }

      /////////////////////////////////////////////////////////////////////////
      // START CLIENT
      // ignore printf error handling
      printf("Client connected from %s:%d...\n",
             inet_ntoa(cliaddress.sin_addr),
             ntohs(cliaddress.sin_port));
      clientCommunication(&new_socket); // returnValue can be ignored
      new_socket = -1;
   }

   // frees the descriptor
   if (create_socket != -1)
   {
      if (shutdown(create_socket, SHUT_RDWR) == -1)
      {
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
char* receive(char* buffer, int *current_socket)
{
   /////////////////////////////////////////////////////////////////////////
      // RECEIVE
      int size;
      size = recv(*current_socket, buffer, BUF - 1, 0);
      if (size == -1)
      {
         if (abortRequested)
         {
            throw invalid_argument("recv error after aborted");
         }
         else
         {
            throw invalid_argument("recv error");
         }
         
      }

      if (size == 0)
      {
         throw invalid_argument("Client closed remote socket"); // ignore error
         
      }

      // remove ugly debug message, because of the sent newline of client
      if (buffer[size - 2] == '\r' && buffer[size - 1] == '\n')
      {
         size -= 2;
      }
      else if (buffer[size - 1] == '\n')
      {
         --size;
      }
      buffer[size] = '\0';
   return buffer;
}

void *clientCommunication(void *data)
{
   char buffer[BUF];
   int *current_socket = (int *)data;
   int isQuit;
   char commands[4][5] = {"send", "list", "read", "del"};

   ////////////////////////////////////////////////////////////////////////////
   // SEND welcome message
   strcpy(buffer, "Welcome to myserver!\r\nPlease enter your commands...\r\n");
   if (send(*current_socket, buffer, strlen(buffer), 0) == -1)
   {
      perror("send failed");
      return NULL;
   }

   do
   {
      int isValid = 1;
      int command = -1;
      try
      {
         strcpy(buffer,receive(buffer, current_socket));
         printf("Message received: %s\n", buffer); // ignore error
      }
      catch (const invalid_argument& except)
      {
         cerr << except.what() << endl;
         break;
      }
      isQuit = strcmp(buffer, "quit") == 0;
      if(!isQuit)
      {
         for(int i = 0; i < 4; i++)
         {
            if(isValid != 0)
            {
               isValid = strcmp(buffer, commands[i]);
               command = i;
            }
         }
         if(isValid == 0)
         {
           if(command == 0)
           {
               char temp[BUF];
               path directorypath;
               path filepath;
               for(int i = 0; i < 3; i++)
               {
                  try
                  {
                     strcpy(buffer,receive(buffer, current_socket));
                     printf("Content received: %s\n", buffer); // ignore error
                     if(i == 0)
                     {
                        strcpy(temp,buffer);
                     }
                     else if(i == 1)
                     {
                        string str = buffer;
                        directorypath = "./mail-spool-directory/" + str;
                        if (!exists(directorypath)) 
                        { 
                           create_directory(directorypath);  
                        }
                        filepath = directorypath / "test.txt";
                        ofstream file(filepath);
                        if (file.is_open()) 
                        { 
                           // Write data to the file 
                           file << temp << endl; //funktioniert noch nicht richtig
                           file << buffer << endl; 
                           file.close(); 
                           cout << "File created: " << filepath << endl; 
                        } 
                        else 
                        { 
                           // Handle the case if any error occured 
                           cerr << "Failed to create file: " << filepath 
                                 << endl; 
                        } 
                     }
                     else if(i == 2)
                     {
                        ofstream file(filepath);
                        if (file.is_open()) 
                        { 
                           // Write data to the file 
                           file << buffer << endl; 
                           file.close(); 
                           cout << "File modified: " << filepath << endl; 
                        } 
                        else 
                        { 
                           // Handle the case if any error occured 
                           cerr << "Failed to create file: " << filepath 
                                 << endl; 
                        } 
                     }
                  }
                  catch (const invalid_argument& except)
                  {
                     cerr << except.what() << endl;
                  }
               }
               while(strcmp(buffer, ".") != 0 &&  strlen(buffer) != 1) 
               {
                  try
                  {
                     strcpy(buffer,receive(buffer, current_socket));
                     printf("Content received: %s\n", buffer); // ignore error
                     if(strcmp(buffer, ".") != 0 &&  strlen(buffer) != 1)
                     {
                        ofstream file(filepath);
                        if (file.is_open()) 
                        { 
                           // Write data to the file 
                           file << buffer << endl; 
                           file.close(); 
                           cout << "File modified: " << filepath << endl; 
                        } 
                        else 
                        { 
                           // Handle the case if any error occured 
                           cerr << "Failed to create file: " << filepath 
                                 << endl; 
                        } 
                     }
                  }
                  catch (const invalid_argument& except)
                  {
                     cerr << except.what() << endl;
                  }
               }
               if (send(*current_socket, "OK", 3, 0) == -1)
               {
                  perror("send answer failed");
                  return NULL;
               }
           }
           else if(command == 1)
           {
               try
               {
                  strcpy(buffer,receive(buffer, current_socket));
                  printf("Content received: %s\n", buffer); // ignore error
               }
               catch (const invalid_argument& except)
               {
                  cerr << except.what() << endl;
               }
               if (send(*current_socket, "OK", 3, 0) == -1)
               {
                  perror("send answer failed");
                  return NULL;
               }
           }
           else if(command == 2)
           {
               for(int i = 0; i < 2; i++)
               {
                  try
                  {
                     strcpy(buffer,receive(buffer, current_socket));
                     printf("Content received: %s\n", buffer); // ignore error
                  }
                  catch (const invalid_argument& except)
                  {
                     cerr << except.what() << endl;
                  }
               }
               if (send(*current_socket, "OK", 3, 0) == -1)
               {
                  perror("send answer failed");
                  return NULL;
               }
           }
           else if(command == 3)
           {
               for(int i = 0; i < 2; i++)
               {
                  try
                  {
                     strcpy(buffer,receive(buffer, current_socket));
                     printf("Content received: %s\n", buffer); // ignore error
                  }
                  catch (const invalid_argument& except)
                  {
                     cerr << except.what() << endl;
                  }
               }
               if (send(*current_socket, "OK", 3, 0) == -1)
               {
                  perror("send answer failed");
                  return NULL;
               }
           }
         }
         else
         {
            if (send(*current_socket, "ERR", 4, 0) == -1)
            {
               perror("send answer failed");
               return NULL;
            }
         }
      }
   } while (!isQuit && !abortRequested);

   // closes/frees the descriptor if not already
   if (*current_socket != -1)
   {
      if (shutdown(*current_socket, SHUT_RDWR) == -1)
      {
         perror("shutdown new_socket");
      }
      if (close(*current_socket) == -1)
      {
         perror("close new_socket");
      }
      *current_socket = -1;
   }

   return NULL;
}

void signalHandler(int sig)
{
   if (sig == SIGINT)
   {
      printf("abort Requested... "); // ignore error
      abortRequested = 1;
      /////////////////////////////////////////////////////////////////////////
      // With shutdown() one can initiate normal TCP close sequence ignoring
      // the reference count.
      // https://beej.us/guide/bgnet/html/#close-and-shutdownget-outta-my-face
      // https://linux.die.net/man/3/shutdown
      if (new_socket != -1)
      {
         if (shutdown(new_socket, SHUT_RDWR) == -1)
         {
            perror("shutdown new_socket");
         }
         if (close(new_socket) == -1)
         {
            perror("close new_socket");
         }
         new_socket = -1;
      }

      if (create_socket != -1)
      {
         if (shutdown(create_socket, SHUT_RDWR) == -1)
         {
            perror("shutdown create_socket");
         }
         if (close(create_socket) == -1)
         {
            perror("close create_socket");
         }
         create_socket = -1;
      }
   }
   else
   {
      exit(sig);
   }
}
