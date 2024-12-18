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
#include <termios.h>

///////////////////////////////////////////////////////////////////////////////

#define BUF 1024
#define PORT 6543
#define IP "127.0.0.1"
#define LEN 6

///////////////////////////////////////////////////////////////////////////////

using namespace std;

///////////////////////////////////////////////////////////////////////////////

string username = "test";

///////////////////////////////////////////////////////////////////////////////

char* input(char* buffer, int length);
void inputSend(int create_socket,char* buffer, int size);
void inputRead(int create_socket,char* buffer, int size);
void inputDelete(int create_socket,char* buffer, int size);
void inputLogin(int create_socket,char* buffer, int size);
char* receive(int create_socket, char* buffer, int size);
void listReceive(int create_socket, char* buffer, int size);
void readReceive(int create_socket, char* buffer, int size);
int getch();
const char* getpass();


///////////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv)
{
   int create_socket;
   char buffer[BUF];
   struct sockaddr_in address;
   int isQuit;
   char commands[LEN][LEN] = {"quit","send", "list", "read", "del", "login"};

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
      cerr << "Address insufficently defined - connect to default address." << IP << ":" << PORT << endl;
      inet_aton(IP, &address.sin_addr);
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
      int isValid = 0;
      int isAuthorised = 0;
      int command = -1;
      if(username != "")   //user is definied and that means authorized
      {
         cout << "Valid Commands: QUIT, SEND, LIST, READ, DEL" << endl;
      }
      else                 //user is not logged in
      {
         cout << "Valid Commands: QUIT, LOGIN" << endl;
      }
      strcpy(buffer,input(buffer, BUF));
      char str[1024] = "";
      strcpy(str, buffer);
      int len =strlen(str);
      for(int i = 0; i < len; i++)
      {
         str[i] = tolower(str[i]);
      }
      isQuit = strcmp(str, "quit") == 0;
      for(int i = 0; i < LEN; i++)
      
      {
         if(isValid != 1)
         {
            isValid = strcmp(str, commands[i]) == 0;
            command = i;
         }
      }
      if(username != ""  && command < 5)
      {
         isAuthorised = 1;
      }
      else if(username == "" && (command == 5 || command == 0))
      {
         isAuthorised = 1;
      }
      //////////////////////////////////////////////////////////////////////
      // SEND DATA
      // https://man7.org/linux/man-pages/man2/send.2.html
      // send will fail if connection is closed, but does not set
      // the error of send, but still the count of bytes sent
      if(isValid)
      {
         if(isAuthorised)
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
            try 
            {
               switch(command)
               {
                  case 0:
                     break;
                  case 1:
                     inputSend(create_socket, buffer, size);
                     break;
                  case 2:
                     break;
                  case 3:
                     inputRead(create_socket, buffer, size);
                     break;
                  case 4:
                     inputDelete(create_socket, buffer, size);
                     break;
                  case 5:
                     inputLogin(create_socket, buffer, size);
                     break;
                  default:
                     throw invalid_argument("Unknown Error");
                     break;
               }
            }
            catch (const invalid_argument& except)
            {
               cerr << except.what() << endl;
               break;
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
            if(!isQuit)
            {
               try 
               {
                  switch(command)
                  {
                     case 0:
                        break;
                     case 1:
                        strcpy(buffer, receive(create_socket, buffer, size));
                        printf("<< %s\n", buffer); // ignore error
                        break;
                     case 2:
                        listReceive(create_socket, buffer, size);
                        break;
                     case 3:
                        readReceive(create_socket, buffer, size);
                        break;
                     case 4:
                        strcpy(buffer, receive(create_socket, buffer, size));
                        printf("<< %s\n", buffer); // ignore error
                        break;
                     case 5:
                        strcpy(buffer, receive(create_socket, buffer, size));;
                        printf("<< %s\n", buffer); // ignore error
                        break;
                     default:
                        throw invalid_argument("Unknown Error");
                        break;
                  }
               }
               catch (const invalid_argument& except)
               {
                  cerr << except.what() << endl;
                  break;
               }
            }
         }
         else if(!isAuthorised)
         {
            switch(command)
            {
               case 0:
                  break;
               case 1:
                  cerr << "Unauthorised Command - Only authenticated Users can use this - Login in to authenicate" << endl;
                  break;
               case 2:
                  cerr << "Unauthorised Command - Only authenticated Users can use this - Login in to authenicate" << endl;
                  break;
               case 3:
                  cerr << "Unauthorised Command - Only authenticated Users can use this - Login in to authenicate" << endl;
                  break;
               case 4:
                  cerr << "Unauthorised Command - Only authenticated Users can use this - Login in to authenicate" << endl;
                  break;
               case 5:
                     cerr << "Already logged in" << endl;
                  break;
            }
         }
      }
      else
      {
         cerr << "Invalid Command" << endl;
      }
   }
   while (!isQuit);

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

char* input(char* buffer, int length)
{
   int size;
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

void inputSend(int create_socket,char* buffer, int size)
{
   cout << "Sender: ";
   cout << username << endl;
   cout << "Receiver: ";
   strcpy(buffer,input(buffer, BUF));
   if ((send(create_socket, buffer, size + 1, 0)) == -1) 
   {
      throw invalid_argument("send error");
   }
   cout << "Subject (max. 80 chars): ";
   strcpy(buffer,input(buffer, 81));
   if ((send(create_socket, buffer, size + 1, 0)) == -1) 
   {
      throw invalid_argument("send error");
   }
   cout << "Message:" << endl;
   while(strcmp(buffer, ".") != 0) 
   {
      cout << ">>";
      strcpy(buffer,input(buffer, BUF));
      if ((send(create_socket, buffer, size + 1, 0)) == -1) 
      {
         throw invalid_argument("send error");
      }
   }
}

void inputRead(int create_socket, char* buffer, int size)
{
   cout << "Message number: ";
   strcpy(buffer,input(buffer, BUF));
   if ((send(create_socket, buffer, size + 1, 0)) == -1) 
   {
      throw invalid_argument("send error");
   }
}

void inputDelete(int create_socket, char* buffer, int size)
{
   cout << "Message number: ";
   strcpy(buffer,input(buffer, BUF));
   if ((send(create_socket, buffer, size + 1, 0)) == -1) 
   {
      throw invalid_argument("send error");
   }
}

void inputLogin(int create_socket, char* buffer, int size)
{
   cout << "Username: ";
   strcpy(buffer,input(buffer, BUF));
   if ((send(create_socket, buffer, size + 1, 0)) == -1) 
   {
      throw invalid_argument("send error");
   }
   strcpy(buffer,getpass());
   if ((send(create_socket, buffer, size + 1, 0)) == -1) 
   {
      throw invalid_argument("send error");
   }
}

char* receive(int create_socket, char* buffer, int size)
{
   size = recv(create_socket, buffer, BUF - 1, 0);
   if (size == -1)
   {
      throw invalid_argument("recv error");
   }
   else if (size == 0)
   {
         throw invalid_argument("Server closed remote socket"); // ignore error
   }
   else
   {
      buffer[size] = '\0';
      if (strcmp("ERR", buffer) == 0)  //if the client gets an error returned from the server
      {                                //it will disconnect from the server
         throw invalid_argument("<< Server error occured, abort");
      }
   }
   return buffer;
}

void listReceive(int create_socket, char* buffer, int size)
{
   strcpy(buffer, receive(create_socket, buffer, size));
   int messageCount = atoi(buffer);
   cout << "Message Count: " << buffer << endl;
   for(int i = 0; i < messageCount; i++)
   {
      strcpy(buffer, receive(create_socket, buffer, size));
      cout << "Subject " << i+1 <<": " << buffer << endl;
   }
}
void readReceive(int create_socket, char* buffer, int size) //reads the message received from the server
{
   strcpy(buffer, receive(create_socket, buffer, size));
   printf("<< %s\n", buffer); // ignore error
   cout << "Sender: " << username << endl;
   strcpy(buffer, receive(create_socket, buffer, size));
   cout << "Receiver: " << buffer << endl;
   strcpy(buffer, receive(create_socket, buffer, size));
   cout << "Subject: " << buffer << endl;
   cout << "Message:" << endl;
   while(strcmp(buffer, ".") != 0) 
   {
      strcpy(buffer, receive(create_socket, buffer, size));
      cout << "<< " << buffer << endl;
   }

}
int getch()
{
    int ch;
    // https://man7.org/linux/man-pages/man3/termios.3.html
    struct termios t_old, t_new;

    // https://man7.org/linux/man-pages/man3/termios.3.html
    // tcgetattr() gets the parameters associated with the object referred
    //   by fd and stores them in the termios structure referenced by
    //   termios_p
    tcgetattr(STDIN_FILENO, &t_old);
    
    // copy old to new to have a base for setting c_lflags
    t_new = t_old;

    // https://man7.org/linux/man-pages/man3/termios.3.html
    //
    // ICANON Enable canonical mode (described below).
    //   * Input is made available line by line (max 4096 chars).
    //   * In noncanonical mode input is available immediately.
    //
    // ECHO   Echo input characters.
    t_new.c_lflag &= ~(ICANON | ECHO);
    
    // sets the attributes
    // TCSANOW: the change occurs immediately.
    tcsetattr(STDIN_FILENO, TCSANOW, &t_new);

    ch = getchar();

    // reset stored attributes
    tcsetattr(STDIN_FILENO, TCSANOW, &t_old);

    return ch;
}

const char *getpass()
{
    int show_asterisk = 1;

    const char BACKSPACE = 127;
    const char RETURN = 10;

    unsigned char ch = 0;
    std::string password;

    printf("Password: ");

    while ((ch = getch()) != RETURN)
    {
        if (ch == BACKSPACE)
        {
            if (password.length() != 0)
            {
                if (show_asterisk)
                {
                    printf("\b \b"); // backslash: \b
                }
                password.resize(password.length() - 1);
            }
        }
        else
        {
            password += ch;
            if (show_asterisk)
            {
                printf("*");
            }
        }
    }
    printf("\n");
    return password.c_str();
}
