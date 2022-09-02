#include<unistd.h>
#include<stdio.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<string.h>
#include<ctype.h>
#include<arpa/inet.h>
#include<errno.h>
#include<stdlib.h>
#include<sys/stat.h>
#include<pthread.h>

#define SERVER_PORT 80

static int debug = 1; //调试开关

void *do_http_request(void *pclient_sock);
int get_line(int sock, char *buf, int size);
void do_http_respanse(int client_sock, const char *path);//响应客户端http
int headers(int client_sock, FILE *resource);
void cat(int client_sock, FILE *resource);
void not_found(int client_sock);//错误信息网页 404
void unimplenmented(int client_sock);//501
void inner_error(int client_sock);//500
void bad_request(int client_sock);//400
int main()
{
    int sock;//sock相当于信箱

    struct sockaddr_in server_addr;//标签

    sock=socket(AF_INET, SOCK_STREAM, 0);//参数2表示用tcp协议,创建信箱

    bzero(&server_addr,sizeof(server_addr));//清空标签，写上地址和端口号
    server_addr.sin_family = AF_INET;//选择协议族ipv4
    server_addr.sin_port = htons(SERVER_PORT);//绑定端口号 h表示host，n表示network，l表示32位长整数，s表示16位短整数。

    bind(sock,(struct sockaddr *)&server_addr, sizeof(server_addr));//实现标签贴到收信的信箱上

    listen(sock,128);//把信箱挂置到传达室，这样就可以接受到信件， 第二个参数表示同时来的信件只接受多少封（128）

    //万事俱备，只等来信
    printf("等待客户端的连接... \n");

    int done =1;
    while(done)
    {
        struct sockaddr_in client;
        int client_sock, len;
        char client_ip[64];
        char buf[256];
	pthread_t id;
	int *pclient_sock = NULL;

        socklen_t client_addr_len;
        client_addr_len = sizeof(client);
        client_sock = accept(sock,(struct sockaddr *)&client, &client_addr_len);//接收客户端请求，尝试和客户端连接

        //打印客户端ip地址和端口号
        printf("client ip: %s\t port : %d\n",
               inet_ntop(AF_INET, &client.sin_addr.s_addr, client_ip, sizeof(client_ip)),
               ntohs(client.sin_port));//ntop p指我们日常写的ip地址
        /*处理http请求，读取客户端发送的数据*/
       // do_http_request(client_sock);

        //启动线程处理http请求
	pclient_sock =(int *)malloc(sizeof(int));
	*pclient_sock = client_sock;
	pthread_create(&id, NULL, do_http_request, (void *)pclient_sock);//创建线程

        //close(client_sock);


        
    }
    close(sock);//关闭sock
    return 0;
}
void *do_http_request(void *pclient_sock)
{
    int len =0;
    char buf[256];
    char method[64];
    char url[512];
    char path[1024];//网页路径长度
    struct stat st;//stat函数里的一个结构体
    int client_sock = *(int *)pclient_sock;

    //*读取客户端发送的http请求*
    //1.读取请求行
//	do{
    len = get_line(client_sock, buf, sizeof(buf));
//	printf("read line:%s\n",buf);
//	}while(len>0);
    if (len > 0) //读到了请求行
    {
        int i = 0, j = 0;
        while (!isspace(buf[j]) && (i < sizeof(method) - 1)) //isspace判断是否为白空格
        {
            method[i] = buf[j];
            i++;
            j++;
        }
        method[i] = '\0';
        if (debug) printf("request method: %s\n", method);

        if (strncasecmp(method, "GET", i) == 0) //只处理get请求 strncasecmp对比字符串（不区分大小写）
        {
            if (debug) printf("method =GET\n");

            //获取url
            while (isspace(buf[j++])); //跳过白空格
            i = 0;

            while (!isspace(buf[j]) && (i < sizeof(url) - 1))
            {
                url[i] = buf[j];
                i++;
                j++;
            }

            url[i] = '\0';
            if (debug) printf("url :%s\n", url);
            //继续读取http头部
            do
            {
                len = get_line(client_sock, buf, sizeof(buf));
                printf("read line:%s\n", buf);
            } while (len > 0);

        
        //***定位服务器本地的文件***

        //处理url的问号（网页参数）问号后面全不要，用结束符替代问号即可
        {
            char* pos = strchr(url, '?');
            if (pos)
            {
                *pos = '\0';
                printf("real url : %s\n", url);
            }
        }

        sprintf(path, "./html_docs/%s", url);//定位到服务器本地文件
        if (debug) printf("path: %s\n", path);

        //执行http响应
	//判断文件是否存在，如果存在就响应200 ok，同时发送相应的html文件，如果不存在，就响应404 not found
	
        if(stat(path,&st) == -1){//文件不存在或是出错
		fprintf(stderr, "stat %s failed. reason :%s\n", path, strerror(errno));
		not_found(client_sock);
	}
	else{//文件存在
		//如果是目录
		if(S_ISDIR(st.st_mode)){
			strcat(path,"/index.html");//?

		}

		do_http_respanse(client_sock,path);
	}

	}
        else //非get请求，读取http头部，并响应客户端错误码501 method not implemented
        {
            fprintf(stderr, "warning! other request[%s]\n", method);

            do
            {
                len = get_line(client_sock, buf, sizeof(buf));
                printf("read line:%s\n", buf);
            } while (len > 0);

            unimplenmented(client_sock); //请求未实现
        }
    }
    else  //请求格式有问题，出错处理
    {
          bad_request(client_sock);
    }
    close(client_sock);//关闭sock
    if(pclient_sock) free(pclient_sock);//释放动态分配的内存

    return NULL;

}


void do_http_respanse(int client_sock, const char *path){//响应客户端http
	int ret = 0;

	FILE *resource =NULL;//定义一个文件类型

	resource = fopen(path,"r");

	if(resource == NULL){
		not_found(client_sock);
		return;
	}

	//1.发送http 头部
	ret = headers(client_sock , resource);
	//2.发送http body
	if(!ret){
		cat(client_sock, resource);
	}
	fclose(resource);
}
/******************************
 * 返回关于响应文件信息的http头部
 * 输入：
 * client_sock -客户端socket句柄
 * resource -文件的句柄
 * 返回值： 成功返回0 失败返回-1
 *****************************/
int headers(int client_sock, FILE *resource){
  	struct stat st;
        int fileid =0;
	char tmp[64];
        char buf[1024]={0};
        
        strcpy(buf,"HTTP/1.0 200 OK\r\n");
        strcat(buf,"Server: Martin Server\r\n");
        strcat(buf,"Content-Type: text/html\r\n");
        strcat(buf,"Connection: Close\r\n");

        fileid = fileno(resource);
	//服务器内部出错处理
        if(fstat(fileid, &st)== -1){//?
                inner_error(client_sock);
		return -1;
        }
        snprintf(tmp, 64, "Content_Length: %ld\r\n\r\n", st.st_size);
        strcat(buf, tmp);

        if(debug) fprintf(stdout,"header : %s\n",buf);
        //把信息发回client——sock
        if(send(client_sock, buf, strlen(buf), 0) < 0){
                fprintf(stderr, "send failed. data: %s,reason: %s\n",buf, strerror(errno));
		return -1;
        }
	return 0;
}
/***********************
 说明：将html文件内容按行
 读取并送给客户端
 * ********************/
void cat(int client_sock, FILE *resource){
	char buf[1024];
	
	//一次读取一行
	fgets(buf, sizeof(buf), resource);
	
	while(!feof(resource)){//判断是否到文件末尾
		int len = write(client_sock, buf, strlen(buf));
		
		if(len<0){//发送body的过程中出现问题
			fprintf(stderr,"send body error. reason: %s\n",strerror(errno));
			break;
		}

		if(debug) fprintf(stdout,"%s", buf);
		fgets(buf, sizeof(buf), resource);

	}	

}
/*响应客户端http
void do_http_respanse(int client_sock)
{
    const char *main_header = "HTTP/1.0 200 OK\r\nServer: Martin Server\r\nContent-Type: text/html\r\nConnection: Close\r\n";
    const char *welcome_content = "\
<html lang=\"zh-CN\">\n\
<head>\n\
<meta content=\"text/html; charset=utf-8\" http-equiv=\"Content-Type\">\n\
<title>This is a test</title>\n\
</head>\n\
<body>\n\
<div align=center height=\"500px\" >\n\
<br/><br/><br/>\n\
<h2>大家好，欢迎来到奇牛学院VIP 试听课！</h2><br/><br/>\n\
<form action=\"commit\" method=\"post\">\n\
尊姓大名: <input type=\"text\" name=\"name\" />\n\
<br/>芳龄几何: <input type=\"password\" name=\"age\" />\n\
<br/><br/><br/><input type=\"submit\" value=\"提交\" />\n\
<input type=\"reset\" value=\"重置\" />\n\
</form>\n\
</div>\n\
</body>\n\
</html>";//注意内部的双引号要打转义字符
    //1.发送 mian_header
    int len =write(client_sock,main_header,strlen(main_header));

    if(debug) fprintf(stdout,"...do_http_response...\n");
    if(debug) fprintf(stdout,"write[%d]: %s",len,main_header);
    //2.动态生成content—length
    char send_buf[64];
    int wc_len = strlen(welcome_content);
    len = snprintf(send_buf,64,"Content_Length: %d\r\n\r\n",wc_len);
    len = write(client_sock, send_buf,len);

    if(debug) fprintf(stdout,"write[%d]: %s",len,send_buf);

    len = write(client_sock, welcome_content, wc_len);
    if(debug) fprintf(stdout,"write[%d]: %s",len,welcome_content);
}*/

//-1 读取出错 0读空行 >0成功读取一行字符数
int get_line(int sock, char *buf, int size)
{
    int count = 0; //计数值
    char ch = '\0';
    int len = 0;

    while((count < size - 1) && ch != '\n')
    {
        len = read(sock, &ch, 1);

        if(len == 1)
        {
            if(ch == '\r')
                continue;
            else if(ch == '\n')
                break;
            //处理一般字符
            buf[count]= ch;
            count++;
        }
        //读取出错
        else if(len ==-1)
        {
            perror("read failed");
            count = -1;
            break;
        }
        //read返回0，客户端关闭了sock连接
        else
        {
            fprintf(stderr, "client close.\n");
            count =-1;
            break;
        }
    }
    if(count>= 0) buf[count]='\0';

    return count;

}

void not_found(int client_sock){
	const char *reply ="HTTP/1.0 404 NOT FOUND\r\n\
Content-Type: text/html\r\n\
\r\n\
<HTML lang=\"zh-CN\">\r\n\
<meta content=\"text/html; charset=utf-8\" http-equiv=\"Content-Type\">\r\n\
<HEAD>\r\n\
<TITLE>NOT FOUND</TITLE>\r\n\
</HEAD>\r\n\
<BODY>\r\n\
	<P>文件不存在！\r\n\
    <P>The server could not fulfill your request because the resource specified is unavailable or nonexistent.\r\n\
</BODY>\r\n\
</HTML>";
	int len =write(client_sock, reply, strlen(reply));
	if(debug) fprintf(stdout,"%s",reply);

	if(len<= 0){
		fprintf(stderr,"send reply failed. reason :%s\n",strerror(errno));
	}
}

void unimplenmented(int client_sock){
	 const char * reply =" HTTP/1.0 501 Method Not Implemented\r\n\
Content-Type: text/html\r\n\
\r\n\
<HTML>\r\n\
<HEAD>\r\n\
<TITLE>Method Not Implemented</TITLE>\r\n\
</HEAD>\r\n\
<BODY>\r\n\
    <P>HTTP request method not supported.\r\n\
</BODY>\r\n\
</HTML>";
        int len = write(client_sock, reply, strlen(reply));
        if(debug) fprintf(stdout,"%s", reply);

        if(len <=0){
                fprintf(stderr, "send reply failed. reason: %s\n", strerror(errno));
        }
}

void bad_request(int client_sock){
        const char * reply = "HTTP/1.0 400 BAD REQUEST\r\n\
Content-Type: text/html\r\n\
\r\n\
<HTML>\r\n\
<HEAD>\r\n\
<TITLE>BAD REQUEST</TITLE>\r\n\
</HEAD>\r\n\
<BODY>\r\n\
    <P>Your browser sent a bad request!\r\n\
</BODY>\r\n\
</HTML>";
        int len = write(client_sock, reply, strlen(reply));
        if(debug) fprintf(stdout,"%s", reply);

        if(len <=0){
                fprintf(stderr, "send reply failed. reason: %s\n", strerror(errno));
        }
}

void inner_error(int client_sock){
	const char * reply = "HTTP/1.0 500 Internal Sever Error\r\n\
Content-Type: text/html\r\n\
\r\n\
<HTML lang=\"zh-CN\">\r\n\
<meta content=\"text/html; charset=utf-8\" http-equiv=\"Content-Type\">\r\n\
<HEAD>\r\n\
<TITLE>Inner Error</TITLE>\r\n\
</HEAD>\r\n\
<BODY>\r\n\
    <P>服务器内部出错.\r\n\
</BODY>\r\n\
</HTML>";

	int len = write(client_sock, reply, strlen(reply));
	if(debug) fprintf(stdout,"%s", reply);
	
	if(len <=0){
		fprintf(stderr, "send reply failed. reason: %s\n", strerror(errno));
	}
}

