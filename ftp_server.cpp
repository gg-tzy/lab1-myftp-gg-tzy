#include <stdio.h>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

//USER NAME and PASSWORD
char user[12];

int listenfd,connfd;
int servport;
char* IP;

//myTCP Header
struct myTCP_Header{
    char m_protocol[6]; /* protocol magic number (6 bytes) */
    char m_type;                          /* type (1 byte) */
    char m_status;                      /* status (1 byte) */
    uint32_t m_length;                    /* length (4 bytes) in Big endian*/
} __attribute__ ((packed));

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
        //std::cout<<buf[i]<<std::endl;
        if(recvHeader.m_protocol[i]!=buf[i]){
            return -1;
        }
    }
    return 1;
}

int serv_connect(){
    socklen_t addrlen;
    struct sockaddr_in cliaddr,servaddr;
    bzero(&cliaddr, sizeof(cliaddr));
    bzero(&servaddr, sizeof(servaddr));

    listenfd= socket(AF_INET,SOCK_STREAM,0);
    servaddr.sin_family=AF_INET;
    servaddr.sin_addr.s_addr= htonl(INADDR_ANY);
    servaddr.sin_port= htons(servport);

    if(bind(listenfd,(struct sockaddr*)&servaddr, sizeof(servaddr))==-1){
        printf("bind failed\n");
        return -1;
    }

    if(listen(listenfd,100)<0){
        printf("listen failed\n");
        return -1;
    }

    addrlen= sizeof(cliaddr);
    connfd= accept(listenfd,(struct sockaddr*)&cliaddr,&addrlen);
    if(connfd<0){
        printf("accept failed\n");
        return -1;
    }

    char buff[INET_ADDRSTRLEN + 1] = {0};
    inet_ntop(AF_INET, &cliaddr.sin_addr, buff, INET_ADDRSTRLEN);
    uint16_t port = ntohs(cliaddr.sin_port);
    printf("connection from %s, port %d\n", buff, port);

    return 0;
}

//handle the request
void serv_handler(myTCP_Header recvHeader,int connfd){
    myTCP_Header sendHeader;
    memcpy(sendHeader.m_protocol,"\xe3myftp", sizeof(sendHeader.m_protocol));
    ssize_t size=0;

    //OPEN request
    if(recvHeader.m_type=='\xA1'){
        sendHeader.m_type='\xA2';
        sendHeader.m_status=1;
        sendHeader.m_length= htonl(12);
        ssize_t size= send(connfd,(struct myTCP_Header*)&sendHeader, sizeof(myTCP_Header),0);
        if(size<0){
            printf("error: send failed\n");
            return;
        } else{
            return;
        }
    }

    //AUTH request
    if(recvHeader.m_type=='\xA3'){
        sendHeader.m_type='\xA4';
        sendHeader.m_length= htonl(12);
        sendHeader.m_status=0;

        if(recvHeader.m_length== htonl(24)){
            char payload[24];
            size= recv(connfd,payload,12,0);
            //std::cout<<payload<<std::endl;
            //std::cout<<strlen(payload)<<std::endl;
            if(strcmp(user,payload)==0){
                sendHeader.m_status=1;
                std::cout<<"right user!"<<std::endl;
            }
        }

        ssize_t size= send(connfd,(struct myTCP_Header*)&sendHeader,12,0);
        if(size<0){
            printf("send failed\n");
            return;
        } else{
            //printf("send succeed\n");
        }

        if(sendHeader.m_status==0){
            printf("connect again\n");
            close(listenfd);
            close(connfd);
            serv_connect();
        }

        return;
    }

    //LIST request
    if(recvHeader.m_type=='\xA5'){
        std::cout<<"open list\n"<<std::endl;
        std::string cmd="ls";
        FILE *list= popen(cmd.c_str(),"r");
        if(!list){
            printf("list error\n");
        }
        char payload[2048]={0};
        fread(payload, sizeof(payload),1,list);
        pclose(list);
        payload[strlen(payload)]='\0';

        sendHeader.m_type='\xA6';
        sendHeader.m_length= htonl(12+ strlen(payload)+1);
        ssize_t size= send(connfd,(struct myTCP_Header*)&sendHeader,12,0);
        if(size<0){
            printf("send failed\n");
            return;
        } else{
            //printf("send succeed\n");
        }

        size=send(connfd,payload, strlen(payload)+1,0);
        if(size<0){
            printf("send payload failed\n");
        }

        return;
    }

    //GET request
    if(recvHeader.m_type=='\xA7'){
        sendHeader.m_type='\xA8';
        sendHeader.m_status=0;

        char filename[1024];
        int filename_lenth= ntohl(recvHeader.m_length)-12;
        int file_size=0;
        size= recv(connfd,filename,filename_lenth,0);
        if(size<0){
            printf("error: recv GET_REQUEST error\n");
            return;
        }

        if(access(filename,F_OK)!=-1){
            sendHeader.m_status=1;
            file_size= FileSize(filename);
            printf("find the file\n");
        }

        sendHeader.m_length= htonl(12);

        size= send(connfd,(struct myTCP_Header*)&sendHeader,12,0);
        if(size<0){
            printf("error: sendHeader failed\n");
        }

        if(sendHeader.m_status==1){
            char buffer[4096];
            int len;

            myTCP_Header FILE_DATA;
            FILE_DATA.m_type='\xFF';
            FILE_DATA.m_length= htonl(12+file_size);
            memcpy(FILE_DATA.m_protocol,"\xe3myftp", sizeof(FILE_DATA.m_protocol));
            size= send(connfd,(struct myTCP_Header*)&FILE_DATA, sizeof(myTCP_Header),0);
            if(size<0){
                printf("error: FILE_DATA send failed\n");
                return;
            }

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
                    int b=send(connfd,buffer+datasent,datasending,0);
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
    }

    //PUT request
    if(recvHeader.m_type=='\xA9'){
        sendHeader.m_type='\xAA';
        sendHeader.m_length= htonl(12);

        char filename[1024];
        int filename_lenth= ntohl(recvHeader.m_length)-12;
        size= recv(connfd,filename,filename_lenth,0);
        if(size<0){
            printf("error: recv filename failed\n");
            return;
        }

        size= send(connfd,(struct myTCP_Header*)&sendHeader, sizeof(myTCP_Header),0);
        if(size<0){
            printf("error: send PUT_REPLY failed\n");
            return;
        }

        myTCP_Header FILE_DATA;
        size= recv(connfd,(struct myTCP_Header*)&FILE_DATA, sizeof(myTCP_Header),0);
        if(size<0){
            printf("error: recv FILE_DATA failed\n");
            return;
        }

        //recv the file
        int filesize= ntohl(FILE_DATA.m_length)-12;
        char buffer[4096];
        FILE* fq;
        fq= fopen(filename,"w");
        int dataRecved=0;
        int dataRecving=filesize;
        while(1){
            memset(buffer,0, sizeof(buffer));
            int b= recv(connfd,buffer,4096,0);
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
        return;
    }

    //QUIT request
    if(recvHeader.m_type=='\xAB'){
        sendHeader.m_type='\xAC';
        sendHeader.m_length= htonl(12);
        ssize_t size= send(connfd,(struct myTCP_Header*)&sendHeader, sizeof(myTCP_Header),0);
        if(size<0){
            printf("error: send failed\n");
        }
        return;
    }
}

int main(int argc, char ** argv) {
    if(argc!=3){
        printf("error\n");
        return 0;
    }

    strcpy(user,"user 123123\0");

    servport= atoi(argv[2]);
    IP=argv[1];

    if(serv_connect()<0)
        return 0;

    for(;;){
        myTCP_Header recvHeader;
        if(recv(connfd,(struct myTCP_Header*)&recvHeader, sizeof(myTCP_Header),0)<0){
            printf("receive failed\n");
        }

        //check whether the protocol is correct
        if(check_protocol(recvHeader)>0){
            serv_handler(recvHeader,connfd);
        } else{
            printf("illegal ftp\n");
        }

    }
    close(connfd);
    close(listenfd);
    return 0;
}
