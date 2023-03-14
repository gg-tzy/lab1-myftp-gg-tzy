#include <stdio.h>
#include <sstream>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#define MAXLINE 4096

int close_client=0;

//client orders
std::string client_order[6]={"open","auth","ls","get","put","quit"};

//myTCP Header
struct myTCP_Header{
    char m_protocol[6]; /* protocol magic number (6 bytes) */
    char m_type;                          /* type (1 byte) */
    char m_status;                      /* status (1 byte) */
    uint32_t m_length;                    /* length (4 bytes) in Big endian*/
} __attribute__ ((packed));

int client_socket;
struct sockaddr_in servaddr;

//get the filesize
int FileSize(const char* filename)
{
    struct stat statbuf;
    if(stat(filename,&statbuf)==0)
        return statbuf.st_size;
    return -1;
}

//check the protocol
int check_protocol(myTCP_Header recvHeader){
    char buf[6];
    memcpy(buf,"\xe3myftp",6);
    for(int i=0;i<6;i++){
        if(recvHeader.m_protocol[i]!=buf[i]){
            return -1;
        }
    }
    return 1;
}

//open a connection to server
void open(const char* IP,const char* port){
    unsigned int SERV_PORT= atoi(port);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_addr.s_addr= inet_addr(IP);
    servaddr.sin_port= htons(SERV_PORT);
    servaddr.sin_family= AF_INET;

    client_socket= socket(AF_INET,SOCK_STREAM,0);
    if(client_socket==-1){
        printf("fail to create client_socket\n");
        return;
    }

    if(connect(client_socket,(struct sockaddr*)&servaddr, sizeof(servaddr))==-1){
        printf("connection failed\n");
        return;
    }

    myTCP_Header OPEN_CONN_REQUEST;
    myTCP_Header OPEN_CONN_REPLY;

    //OPEN_CONN_REQUEST
    OPEN_CONN_REQUEST.m_length=htonl(12);
    OPEN_CONN_REQUEST.m_type='\xA1';
    memcpy(OPEN_CONN_REQUEST.m_protocol,"\xe3myftp", sizeof(OPEN_CONN_REQUEST.m_protocol));
    //printf("%s\n",OPEN_CONN_REQUEST.m_protocol);
    //std::cout<<OPEN_CONN_REQUEST.m_type<<std::endl;
    //send OPEN_CONN_REQUEST
    ssize_t size=send(client_socket,(struct myTCP_Header*) &OPEN_CONN_REQUEST, sizeof(OPEN_CONN_REQUEST),0);
    if(size<0){
        printf("error: send failed\n");
    } else{
        //std::cout<<size<<std::endl;
    }

    //receive OPEN_CONN_REPLY
    size= recv(client_socket,(struct myTCP_Header*)&OPEN_CONN_REPLY, sizeof(OPEN_CONN_REPLY),0);
    if(size<0){
        printf("error: recv failed\n");
    } else{
        //std::cout<<size<<std::endl;
    }

    if(check_protocol(OPEN_CONN_REPLY)<0){
        printf("illegal ftp\n");
        return;
    }
    if(OPEN_CONN_REPLY.m_type=='\xA2'&&OPEN_CONN_REPLY.m_status==1){
        printf("connection succeed!\n");
        return;
    } else{
        printf("connection failed...\n");
        return;
    }
}

//send the username and the password to server
void auth(const char* username,const char* password){
    int payload_length= strlen(username)+ strlen(password)+ 2;
    char payload[1024];
    int index=0;
    for(int i=0;i< strlen(username);i++)
        payload[index++]=username[i];
    payload[index++]=' ';
    for(int i=0;i< strlen(password);i++)
        payload[index++]=password[i];
    payload[index]='\0';

    myTCP_Header AUTH_REQUEST;
    myTCP_Header AUTH_REPLY;

    //AUTH_REQUEST
    AUTH_REQUEST.m_length=htonl(12+ payload_length);
    AUTH_REQUEST.m_status='\0';
    AUTH_REQUEST.m_type='\xA3';
    memcpy(AUTH_REQUEST.m_protocol,"\xe3myftp", sizeof(AUTH_REQUEST.m_protocol));

    char buf_send[128]={0};
    memcpy(buf_send,&AUTH_REQUEST,12);

    ssize_t size=send(client_socket,buf_send, sizeof(myTCP_Header),0);
    if(size<0){
        printf("error: send failed\n");
        return;
    } else{
        //std::cout<<size<<std::endl;
    }

    size= send(client_socket,payload, payload_length,0);
    //std::cout<<size<<std::endl;
    if(size<0){
        printf("error: payload send failed\n");
        return;
    }

    //receive AUTH_REPLY
    size= recv(client_socket,(struct myTCP_Header*)&AUTH_REPLY, sizeof(myTCP_Header),0);

    if(size<0){
        printf("error: recv failed\n");
        return;
    } else{
        std::cout<<size<<std::endl;
    }

    if(check_protocol(AUTH_REPLY)<0){
        printf("illegal ftp\n");
        return;
    }
    if(AUTH_REPLY.m_type=='\xA4'&&AUTH_REPLY.m_status==1){
        printf("successful AUTH\n");
        return;
    } else if (AUTH_REPLY.m_type=='\xA4'&&AUTH_REPLY.m_status==0){
        printf("AUTH failed\n");
        close(client_socket);
        return;
    }
    return;
}

//get the file list from server
void ls(){
    myTCP_Header LIST_REQUEST;
    myTCP_Header LIST_REPLY;

    //LIST_REQUEST
    LIST_REQUEST.m_length=htonl(12);
    LIST_REQUEST.m_type='\xA5';
    memcpy(LIST_REQUEST.m_protocol,"\xe3myftp", sizeof(LIST_REQUEST.m_protocol));

    ssize_t size=send(client_socket,(struct myTCP_Header*)&LIST_REQUEST, sizeof(myTCP_Header),0);
    if(size<0){
        printf("error: send failed\n");
        return;
    } else{
        //std::cout<<size<<std::endl;
    }

    //receive LIST_REPLY
    size= recv(client_socket,(struct myTCP_Header*)&LIST_REPLY, sizeof(myTCP_Header),0);
    if(size<0){
        printf("error: recv failed\n");
        return;
    } else{
        std::cout<<size<<std::endl;
    }
    if(check_protocol(LIST_REPLY)<0){
        printf("illegal ftp\n");
        return;
    }

    //receive payload
    char payload[2048]={0};
    size= recv(client_socket,payload, sizeof(payload),0);
    if(size<0){
        printf("error: recv failed\n");
        return;
    } else{
        std::cout<<payload;
        return;
    }
}

//download the <filename>
void get(const char* filename){
    myTCP_Header GET_REQUEST;
    myTCP_Header GET_REPLY;
    myTCP_Header FILE_DATA;

    int payload_lenth= strlen(filename)+1;
    char payload[1024];
    for(int i=0;i< strlen(filename);i++){
        payload[i]=filename[i];
    }
    payload[payload_lenth]='\0';

    GET_REQUEST.m_type='\xA7';
    GET_REQUEST.m_length= htonl(12+payload_lenth);
    memcpy(GET_REQUEST.m_protocol,"\xe3myftp", sizeof(GET_REQUEST.m_protocol));

    //send GET_REQUEST
    ssize_t size= send(client_socket,(struct myTCP_Header*)&GET_REQUEST, sizeof(myTCP_Header),0);
    if(size<0){
        printf("error: send failed\n");
    }

    size= send(client_socket,payload,payload_lenth,0);
    if(size<0){
        printf("error: filename send failed\n");
    }

    //receive GET_REPLY
    size= recv(client_socket,(struct myTCP_Header*)&GET_REPLY, sizeof(myTCP_Header),0);

    if(size<0){
        printf("error: recv failed\n");
        return;
    }

    if(check_protocol(GET_REPLY)<0){
        printf("illegal ftp\n");
        return;
    }

    if(GET_REPLY.m_type=='\xA8'&&GET_REPLY.m_status==0){
        printf("file cannot find\n");
        return;
    }

    if(GET_REPLY.m_type=='\xA8'&&GET_REPLY.m_status==1){
        printf("find the file\n");
        int filesize;

        size= recv(client_socket,(struct myTCP_Header*)&FILE_DATA, sizeof(myTCP_Header),0);
        if(size<0){
            printf("error: FILE_DATA recv failed\n");
        }
        filesize= ntohl(FILE_DATA.m_length)-12;
        std::cout<<filesize<<std::endl;

        char buffer[4096];
        FILE* fq;
        fq= fopen(filename,"w");
        int dataRecved=0;
        int dataRecving=filesize;
        while(1){
            memset(buffer,0, sizeof(buffer));
            int b= recv(client_socket,buffer,4096,0);
            std::cout<<b<<std::endl;
            std::cout<<dataRecving<<std::endl;
            if(b==dataRecving){
                printf("send over\n");
                fwrite(buffer,1,b,fq);
                fclose(fq);
                break;
            }else if(b>0){
                fwrite(buffer,1,b,fq);
                dataRecved+=b;
                dataRecving-=b;
                continue;
            } else if(b<=0){
                printf("recv error\n");
                break;
            }
        }
    }
    return;
}

//upload the <filename> to server
void put(const char* filename){
    myTCP_Header PUT_REQUEST;
    myTCP_Header PUT_REPLY;
    myTCP_Header FILE_DATA;

    if(access(filename,F_OK)==-1){
        printf("error: cannot find file: %s\n",filename);
        return;
    }

    //send PUT_REQUEST
    PUT_REQUEST.m_type='\xA9';
    PUT_REQUEST.m_length= htonl(12+ strlen(filename)+1);
    memcpy(PUT_REQUEST.m_protocol,"\xe3myftp", sizeof(PUT_REQUEST.m_protocol));
    ssize_t size= send(client_socket,(struct myTCP_Header*)&PUT_REQUEST, sizeof(myTCP_Header),0);
    if(size<0){
        printf("error: send PUT_REQUEST failed\n");
        return;
    }

    //send filename
    size= send(client_socket,filename, strlen(filename)+1,0);
    if(size<0){
        printf("error: send filename failed\n");
        return;
    }

    //recv PUT_REPLY
    size= recv(client_socket,(struct myTCP_Header*)&PUT_REPLY, sizeof(myTCP_Header),0);
    if(size<0){
        printf("error: recv PUT_REPLY error\n");
        return;
    }

    if(check_protocol(PUT_REPLY)<0){
        printf("illegal ftp\n");
        return;
    }

    if(PUT_REPLY.m_type!='\xAA'){
        printf("error: reply type is wrong\n");
        return;
    }

    int file_size=0;
    file_size= FileSize(filename);
    FILE_DATA.m_type='\xFF';
    FILE_DATA.m_length= htonl(12+file_size);
    memcpy(FILE_DATA.m_protocol,"\xe3myftp", sizeof(FILE_DATA.m_protocol));
    size= send(client_socket,(struct myTCP_Header*)&FILE_DATA, sizeof(myTCP_Header),0);
    if(size<0){
        printf("error: send FILE_DATA failed\n");
        return;
    }

    //send the file
    char buffer[4096];
    int len=0;
    FILE* fq;
    fq= fopen(filename,"r");
    bzero(buffer, sizeof(buffer));
    while(!feof(fq)){
        printf("file data sending\n");
        len= fread(buffer,1, sizeof(buffer),fq);
        std::cout<<len<<std::endl;
        int datasent=0;
        int datasending=len;
        while(1){
            int b=send(client_socket,buffer+datasent,datasending,0);
            std::cout<<b<<std::endl;
            if(b==datasending){
                break;
            } else if(b>0){
                datasending-=b;
                datasent+=b;
                continue;
            } else if(b<0){
                continue;
            }
        }
    }
    fclose(fq);
    return;
}

//cut down the connection and close client
int quit(){
    myTCP_Header QUIT_REQUEST;
    myTCP_Header QUIT_REPLY;

    //send QUIT_REQUEST
    QUIT_REQUEST.m_type='\xAB';
    QUIT_REQUEST.m_length= htonl(12);
    memcpy(QUIT_REQUEST.m_protocol,"\xe3myftp", sizeof(QUIT_REQUEST.m_protocol));
    ssize_t size=send(client_socket,(struct myTCP_Header*)&QUIT_REQUEST, sizeof(myTCP_Header),0);
    if(size<0){
        printf("error: send failed\n");
        return 0;
    }

    //receive QUIT_REPLY
    size= recv(client_socket,(struct myTCP_Header*)&QUIT_REPLY, sizeof(myTCP_Header),0);

    if(size<0){
        printf("error: recv failed\n");
        return 0;
    }

    if(check_protocol(QUIT_REPLY)<0){
        printf("illegal ftp\n");
        return 0;
    }

    if(QUIT_REPLY.m_type=='\xAC'){
        if(close(client_socket)<0){
            printf("close failed\n");
            return 0;
        }else{
            return 1;
        }
    }
    return 0;
}

//handle the order
void handler(char* buff){
    std::istringstream order(buff);
    int argc=0;
    std::string argv[5];
    std::string temp;
    while(order>>temp){
        //std::cout<<temp<<std::endl;
        argv[argc++]=temp;
    }
    argv[argc]="\0";

    if(argc>3){
        printf("error!please try again\n");
        return;
    }
    if(argc<1)  return;

    //open
    if(argv[0]==client_order[0]){
        if(argc!=3){
            printf("parameter error\n");
            return;
        }
        open(argv[1].c_str(),argv[2].c_str());
        return;
    }

    //auth
    if(argv[0]==client_order[1]){
        if(argc!=3){
            printf("parameter error\n");
            return;
        }
        auth(argv[1].c_str(),argv[2].c_str());
        return;
    }

    //ls
    if(argv[0]==client_order[2]){
        if(argc!=1){
            printf("parameter error\n");
            return;
        }
        ls();
        return;
    }

    //get
    if(argv[0]==client_order[3]){
        if(argc!=2){
            printf("parameter error\n");
            return;
        }
        get(argv[1].c_str());
        return;
    }

    //put
    if(argv[0]==client_order[4]){
        if(argc!=2){
            printf("parameter error\n");
            return;
        }
        put(argv[1].c_str());
        return;
    }

    //quit
    if(argv[0]==client_order[5]){
        if(argc!=1){
            printf("parameter error\n");
            return;
        }
        close_client=quit();
        return;
    }

    else{
        std::cout<<"cannot find order '"<<argv[0]<<"'"<<std::endl;
        std::cout<<"please try again..."<<std::endl;
        return;
    }

    return;
}

int main(int argc, char ** argv) {
    while(1){
        printf(">");
        char buff[1024]={0};
        fflush(stdout);
        if(scanf("%[^\n]%*c",buff)!=1)
            getchar();
        handler(buff);
        if(close_client) break;
    }
    return 0;
}
