#include<arpa/inet.h>
#include<unistd.h>
#include<stdlib.h>
#include<string.h>
#include<stdio.h>
#include<sys/select.h>
#include<pthread.h>
pthread_mutex_t mutex;
typedef struct fdInfo{
    int fd;
    int* maxfd;
    fd_set* rdset;
}FdInfo;
void *acceptconn(void* arg){
        FdInfo* info=(FdInfo*)arg;
        struct sockaddr_in addr;
        socklen_t size=sizeof(addr);
        int cfd=accept(info->fd,(sockaddr*)&addr,&size);
        if(cfd==-1){
        perror("accept");
        return 0;
          }
          pthread_mutex_lock(&mutex);
          //将其添加到读集合
          FD_SET(cfd,info->rdset);
           // 重置最大的文件描述符
           *info-> maxfd = cfd > *info->maxfd ? cfd :*info->maxfd;
           pthread_mutex_unlock(&mutex);
           free(info);
           return nullptr;
}
void* communicate(void* arg){
           FdInfo*info=(FdInfo*)arg;
           char buff[1024];
            //char buff[10]={0};
            // 一次只能接收10个字节, 客户端一次发送100个字节
            // 一次是接收不完的, 文件描述符对应的读缓冲区中还有数据
            // 下一轮select检测的时候, 内核还会标记这个文件描述符缓冲区有数据 -> 再读一次
            // 	循环会一直持续, 知道缓冲区数据被读完位置
            int len=recv(info->fd,buff,sizeof(buff),0);
            if(len>0){
            printf("client says:%s\n",buff);
            send(info->fd,buff,sizeof(buff),0);
            }else if(len==0){
            printf("客户端已断开连接。。。。");
            pthread_mutex_lock(&mutex);
            FD_CLR(info->fd,info->rdset);
            pthread_mutex_unlock(&mutex);
            close(info->fd);
            free(info);
            return nullptr;
            }else{
             perror("recv");
             free(info);
            return nullptr;
            }
            free(info);
            return nullptr;
}
   
int  main(){
  pthread_mutex_init(&mutex,nullptr);
    //创建套接字
    int fd=socket(AF_INET,SOCK_STREAM,0);
    if(fd==-1){
        perror("socket");
        return 0;
    }
    //绑定ip
    struct sockaddr_in saddr;
    saddr.sin_port=htons(9999);//转换为大端
    saddr.sin_family=AF_INET;
    saddr.sin_addr.s_addr=INADDR_ANY;
    int ret=bind(fd,(sockaddr*)&saddr,sizeof(saddr));
    if(ret==-1){
    perror("bind");
    return 0;
    }
    //设置监听
    ret=listen(fd,1024);
    if(ret==-1){
        perror("listen");
    }
    //将监听的fd委托给内核
    int maxfd=fd;
    fd_set rdset;
    fd_set tmpset;
    //清零
    FD_ZERO(&rdset);
    //将监听的fd,设置到读集合中
    FD_SET(fd,&rdset);

    //进行通信
    while(true){
        //rdset中是所要要检测的文件描述符
        pthread_mutex_lock(&mutex);
       tmpset=rdset;
       pthread_mutex_unlock(&mutex);
       int num=select(maxfd+1,&tmpset,NULL,nullptr,nullptr);
       // rdset中的数据被内核改写了, 只保留了发生变化的文件描述的标志位上的1, 没变化的改为0
        // 只要rdset中的fd对应的标志位为1 -> 缓冲区有数据了
        // 判断有没有新连接
        if(FD_ISSET(fd,&tmpset)){
          FdInfo* info=(FdInfo*)malloc(sizeof(FdInfo));
          info->fd=fd;
          info->maxfd=&maxfd;
          info->rdset=&rdset;
            //创建线程
           pthread_t tid;
           pthread_create(&tid,nullptr,acceptconn,info);
           pthread_detach(tid);
      }
      //没有新连接，通信
      for(int i=0;i<maxfd+1;i++){
        if(i!=fd&&FD_ISSET(i,&rdset)){
          FdInfo* info=(FdInfo*)malloc(sizeof(FdInfo));
          info->fd=i;
          info->rdset=&rdset;
          pthread_t tid;
          pthread_create(&tid,nullptr,communicate,info);
          pthread_detach(tid);          
        }
      }
    }
    pthread_mutex_destroy(&mutex);
    return 0;
};