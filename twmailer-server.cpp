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
#include <time.h>
#include <stdexcept>
#include <filesystem> 
#include <fstream> 
#include <iostream>
#include <vector>
#include "twmailer-idap.h"

///////////////////////////////////////////////////////////////////////////////

#define BUF 1024
#define PORT 6543
#define LEN 6

///////////////////////////////////////////////////////////////////////////////

using namespace std;
using namespace std::filesystem;

///////////////////////////////////////////////////////////////////////////////

int abortRequested = 0;
int create_socket = -1;
int new_socket = -1;
string spoolDirectoryPath = "";

///////////////////////////////////////////////////////////////////////////////

char* receive(char* buffer, int *current_socket);
void sendMessage(char* buffer,path directorypath,vector<string> index,string user, int* current_socket);
void listMessages(char* buffer,path directorypath,vector<string> index, int* current_socket);
void readMessage(char* buffer,path directorypath,vector<string> index, int* current_socket);
void deleteMessage(char* buffer, path directorypath,vector<string> index, int* current_socket);
void *clientCommunication(void *data);
void signalHandler(int sig);

///////////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv)
{
   socklen_t addrlen;
   struct sockaddr_in address, cliaddress;
   string spoolDirectory = "";
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

   if (setsockopt(create_socket,
                  SOL_SOCKET,
                  SO_REUSEPORT,
                  &reuseValue,
                  sizeof(reuseValue)) == -1)
   {
      perror("set socket options - reusePort");
      return EXIT_FAILURE;
   }

   ////////////////////////////////////////////////////////////////////////////
   // INIT ADDRESS
   // Attention: network byte order => big endian
   memset(&address, 0, sizeof(address));
   address.sin_family = AF_INET;
   address.sin_addr.s_addr = INADDR_ANY;
   if (argc < 3)
   {
      cerr << "Server insufficently defined - Start Server at default port " << PORT << " with mail-spool-directory as spool directory" << endl;
      address.sin_port = htons(PORT);
      spoolDirectory = "mail-spool-directory";
   }
   else
   {
      address.sin_port = htons(atoi(argv[1]));
      spoolDirectory = argv[2];
   }

   spoolDirectoryPath = "./" + spoolDirectory;
   if (!exists(spoolDirectoryPath))
   {
      try
      {
      create_directory(spoolDirectoryPath);
      }
      catch (...)
      {
         cerr << "failed to create directory" << endl;
      }
   }

   ////////////////////////////////////////////////////////////////////////////
   // ASSIGN AN ADDRESS WITH PORT TO SOCKET
   if (bind(create_socket, (struct sockaddr *)&address, sizeof(address)) == -1)
   {
      perror("bind error");
      return EXIT_FAILURE;
   }

   std::cout << "Started Server at port " << ntohs(address.sin_port) << " with " << spoolDirectory << " as the spool directory" << endl;

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
      if ((new_socket = accept(create_socket, (struct sockaddr *)&cliaddress, &addrlen)) == -1)
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
void sendMessage(char* buffer,path directorypath,vector<string> index,string user, int* current_socket)
{
   // sends Message to the server
   string receiver;
   string subject;
   vector<string> messagetext;
   path filepath;
   try
   {
      strcpy(buffer,receive(buffer, current_socket));
      printf("Content received: %s\n", buffer); // ignore error
   }
   catch (const invalid_argument& except)
   {
      cerr << except.what() << endl;
   }
   receiver = buffer;
   try
   {
      strncpy(buffer,receive(buffer, current_socket),81);
      printf("Content received: %s\n", buffer); // ignore error
   }
   catch (const invalid_argument& except)
   {
      cerr << except.what() << endl;
   }
   subject = buffer;
   while(strcmp(buffer, ".") != 0 &&  strlen(buffer) != 1) 
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
      if(strlen(buffer) != 0)
      {
         messagetext.push_back(buffer);
      }
   }
   //We use the the current + the username to give each mail a unique name
   time_t timer;
   time(&timer);
   string filename = user+to_string(timer)+".txt";
   filepath = directorypath/filename;
   index.push_back(filename);
   ofstream file(filepath);
   try
   {
      if (file.is_open()) 
      { 
         // Write data to the file 
         file << receiver << endl;
         file << subject << endl;
         for(long unsigned int i = 0; i != messagetext.size(); i++)
         {
            file << messagetext[i] << endl;
         }
         file.close(); 
         std::cout << "File created: " << filepath << endl; 
      }
   }
   catch (...)
   {
      cerr << "failed to create file" << endl;
   }
   if (send(*current_socket, "OK", 3, 0) == -1)
   {
      perror("send answer failed");
   }
}
void listMessages(char* buffer,path directorypath,vector<string> index, int* current_socket)
{
   int messagecount = 0;
   vector<string> messages;
   if(!filesystem::is_empty(directorypath))
   {
      for(long unsigned int i = 0; i < index.size();i++)
      {
         string subject;
         ifstream message(directorypath/index[i]);
         int counter = 0;
         //We iterate through the mail taking the subject from each
         while (getline (message, subject)) 
         {
            if(counter == 1)
            {
               messages.push_back(subject);
               break;
            }
            counter++;
         }
         message.close();
         messagecount++;
      }
      std::cout << messagecount << endl;
      //We are sending the count of messages to the client
      if (send(*current_socket, to_string(messagecount).c_str(), 3, 0) == -1)
      {
         perror("send answer failed");
      }
      if(messagecount != 0)
      {
         for(int i = 0; i < messagecount;i++)
         {
            std::cout << messages[i] << endl;
            strcpy(buffer,messages[i].c_str());
            if (send(*current_socket,buffer, 3, 0) == -1)
            {
               perror("send answer failed");
            }
         }
         messages.clear();
      }
   }
   else
   {
      if (send(*current_socket, to_string(messagecount).c_str(), 3, 0) == -1)
      {
         perror("send answer failed");
      }
   }
}
void readMessage(char* buffer,path directorypath,vector<string> index, int* current_socket)
{
   string messageNumber;
   long unsigned int messNum = 0;
   if(!filesystem::is_empty(directorypath))
   {
      try
      {
         strcpy(buffer,receive(buffer, current_socket));
         messageNumber = buffer;
         printf("Content received: %s\n", buffer); // ignore error
      }
      catch (const invalid_argument& except)
      {
         cerr << except.what() << endl;
      }
      try
      {
         int temp = stoi(messageNumber);
         if(temp >= 0)
         {
            messNum += temp;
         }
         else
         {
            throw(std::invalid_argument(" "));
         }
      }
      catch(...)
      {
         if (send(*current_socket, "ERR", 4, 0) == -1)
         {
            perror("send answer failed");
         }
      }
      if(messNum <= index.size()-1)
      {
         if (send(*current_socket, "OK", 3, 0) == -1)
         {
            perror("send answer failed");
         }
         string text;
         ifstream file(directorypath/index[messNum-1]);
         while (getline (file, text)) 
         {
            std::cout << text << endl;
            if (send(*current_socket,text.c_str(), 3, 0) == -1)
            {
               perror("send answer failed");
            }
         }
         file.close();
      }
      else
      {
         if (send(*current_socket, "ERR", 4, 0) == -1)
         {
            perror("send answer failed");
         }
      }
   }
   else
   {
      if (send(*current_socket, "ERR", 4, 0) == -1)
      {
         perror("send answer failed");
      }
   }
}
void deleteMessage(char* buffer, path directorypath,vector<string> index, int* current_socket)
{
   string messageNumber;
   long unsigned int messNum = 0;
   if(!filesystem::is_empty(directorypath))
   {
      try
      {
         strcpy(buffer,receive(buffer, current_socket));
         messageNumber = buffer;
         printf("Content received: %s\n", buffer); // ignore error
      }
      catch (const invalid_argument& except)
      {
         cerr << except.what() << endl;
      }
      try
      {
         int temp = stoi(messageNumber);
         if(temp >= 0)
         {
            messNum = temp;
         }
         else
         {
            throw(std::invalid_argument(" "));
         }
      }
      catch(...)
      {
         if (send(*current_socket, "ERR", 4, 0) == -1)
         {
            perror("send answer failed");
         }
      }
      if(messNum <= index.size()-1)
      {
         string fileToRemove = index[messNum];
         vector<string> temp;
         for(long unsigned int i = 0; i != index.size();i++)
         {
            if(i != messNum)
            {
               temp.push_back(index[i]);
            }
         }
         index.clear();    //deleting contents of vector so the referenz to the deleted file can be removed
         index.resize(1);  //resize the vector index to avoid segmentaion faults
         for(long unsigned int i = 0; i != temp.size();i++)
         {
            index.push_back(temp[i]);  //repopulating the vector with the remaining filenames
         }
         remove(directorypath/fileToRemove); //deletes the targeted file
         if (send(*current_socket, "OK", 3, 0) == -1)
         {
            perror("send answer failed");
         }
      }
      else
      {
         if (send(*current_socket, "ERR", 4, 0) == -1)
         {
            perror("send answer failed");
         }
      }
   }
   else
   {
      if (send(*current_socket, "ERR", 4, 0) == -1)
      {
         perror("send answer failed");
      }
   }
}
void *clientCommunication(void *data)
{
   char buffer[BUF];
   int *current_socket = (int *)data;
   int isQuit;
   char commands[LEN][LEN] = {"quit","send", "list", "read", "del", "login"};

   ////////////////////////////////////////////////////////////////////////////
   // SEND welcome message
   strcpy(buffer, "Welcome to twmailer!\r\nPlease enter your commands...\r\n");
   if (send(*current_socket, buffer, strlen(buffer), 0) == -1)
   {
      perror("send failed");
      return NULL;
   }

   do
   {
      int isValid = 0;
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
      char str[1024] = "";
      strcpy(str, buffer);
      int len =strlen(str);
      for(int i = 0; i < len; i++)//all characters are converted to lowercase
      {
         str[i] = tolower(str[i]);
      }
      isQuit = strcmp(str, commands[0]) == 0;
      if(!isQuit)
      {
         for(int i = 1; i < LEN; i++)
         {
            if(isValid != 1)
            {
               isValid = strcmp(str, commands[i]) == 0;     //determines which command is selected
               command = i;
            }
         }
         if(isValid)
         {
            string user = "test";
            path directorypath;
            vector<string> index;
            directorypath = spoolDirectoryPath + "/" + user;
            if (!exists(directorypath)) 
            { 
               try
               {
                  create_directory(directorypath);
               }
               catch (...)
               {
                  cerr << "failed to create directory" << endl;
               }
            }
            if(!empty(directorypath))
            {
               for (auto const& dir_entry : std::filesystem::directory_iterator{directorypath})
               {
                  index.push_back(dir_entry.path().filename());
                  cout << dir_entry.path().filename();
               }
            }
            switch (command)
            {
            case 1:
               sendMessage(buffer,directorypath,index,user,current_socket);
               break;
            case 2:
               listMessages(buffer,directorypath,index,current_socket);
               break;
            case 3:
               readMessage(buffer,directorypath,index,current_socket);
               break;
            case 4:
               deleteMessage(buffer,directorypath,index,current_socket);
               break;
            case 5:
               if (send(*current_socket, "OK", 3, 0) == -1)
               {
                  perror("send answer failed");
                  return NULL;
               }
               break;
            }
         }
         else
         {
            if (send(*current_socket, "ERR", 4, 0) == -1)
            {
               perror("send answer failed");
               break;
            }
         }
      }
   }
   while(!isQuit && !abortRequested);

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
