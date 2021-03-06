#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#include "rtspdef.h"
#include "rtsplog.h"
#include "rtsplib.h"
#include "sock.h"


static int server_fd=-1;
static int toggle=true;
static int rtsp_user_num=0;

static void signal_handle(int sign_no) 
{
    toggle=false;
    RTSP_DEBUG("rtsp server kill!");
    close(server_fd);
    sleep(2);
    exit(0);
}

void* rtsp_server_process(void *para) 
{
	int ret;
	int *sock=(int *)para;
	Rtsp_t r;
	RtpPacket_t rtpp;
	RtspStream_t *stream=&r.s;
	uint32_t ts_base;
	int out_success= false;
	int bStreamInit=false;
	fd_set read_set;
	fd_set write_set;
	struct timeval timeout={0,10*1000};
	
    pthread_detach(pthread_self());
    RTSP_init(&r, *sock);

	while(1){
		FD_ZERO(&read_set);
		FD_SET(r.sock,&read_set);
		ret=select(r.sock+1,&read_set,NULL,NULL,&timeout);
		if(ret < 0){
			RTSP_ERR("select failed");
			break;
		}else if(ret == 0){
			//
		}else{
			if(FD_ISSET(r.sock,&read_set)){
				if(RTSP_read_message(&r)==RTSP_RET_FAIL){
					break;
				}
				if(RTSP_parse_message(&r) == RTSP_RET_FAIL){
					break;
				}
			}

		}// end select
		if(r.state == RTSP_STATE_PLAYING){
			out_success = false;
			if(bStreamInit == false){
				if(RTSP_STREAM_init(stream,DEFAULT_STREAM)<0)
					break;
				bStreamInit = true;
			}
			if(RTSP_STREAM_next(stream)==RTSP_RET_OK){
				if(RTSP_send_frame(&r,stream->data,stream->size,stream->timestamp)==RTSP_RET_FAIL)
					break;
				else
					out_success = true;
			}
			if(out_success){
				usleep(stream->inspeed - 10*1000);
				out_success = false;
			}
		}
	}// end of while
	
	RTSP_destroy(&r);
	
	return NULL;
}


int rtsp_server_main(int argc,char *argv[]) {
    int client_fd;
    signal(SIGINT,signal_handle);
    server_fd=tcp_server_init(RTSP_DEFAULT_PORT);
    do {
        struct sockaddr client;
        socklen_t size=sizeof(struct sockaddr);
        if ((client_fd = accept(server_fd,(struct sockaddr *)&client,&size)) == -1) {
            RTSP_ERR("accept failed, errno=%d!!!",errno);
            exit(1);
        } else {
            struct sockaddr_in sin=*((struct sockaddr_in *)&client);
            RTSP_INFO("accept connect from:%s sockfd:%d",inet_ntoa(sin.sin_addr),client_fd);

            pthread_t tid=0;
            if (pthread_create(&tid,NULL,rtsp_server_process,(void *)&client_fd) != 0) {
                RTSP_ERR("create thread failed!");
                exit(1);
            } else {
                RTSP_INFO("create thread success , tid=%d",(int)tid);
            }
        }
    } while (toggle);

    return 0;
}



