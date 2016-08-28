#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <linux/sched.h>
#include <linux/genetlink.h>
#include "kernel_comm.h"
#include "smv_lib.h"

#define GENLMSG_DATA(glh) ((void *)(NLMSG_DATA(glh) + GENL_HDRLEN))
#define GENLMSG_PAYLOAD(glh) (NLMSG_PAYLOAD(glh, 0) - GENL_HDRLEN)
#define NLA_DATA(na) ((void *)((char*)(na) + NLA_HDRLEN))

static int create_netlink_socket(int groups){
    socklen_t addr_len;
    int sd = socket(AF_NETLINK, SOCK_RAW, NETLINK_GENERIC);
    if(sd < 0){
        rlog("cannot create netlink socket");
        return -1;
    }
    
    struct sockaddr_nl local;
    memset(&local, 0, sizeof(local));
    local.nl_family = AF_NETLINK;
    local.nl_groups = groups;
    
    int rc = bind(sd, (struct sockaddr *) &local, sizeof(local));
    if(rc < 0){
        rlog("cannot bind netlink socket");
        close(sd);
        return -1;
    }
    
    return sd;
}

int get_family_id(int netlink_socket){
    struct {
        struct nlmsghdr n;
        struct genlmsghdr g;
        char buf[256];
    } family_req;
    
    struct {
        struct nlmsghdr n;
        struct genlmsghdr g;
        char buf[256];
    } ans;
    
    int id;
    struct nlattr *na;
    int rep_len;
    
    /* Get family name */
    family_req.n.nlmsg_type = GENL_ID_CTRL;
    family_req.n.nlmsg_flags = NLM_F_REQUEST;
    family_req.n.nlmsg_seq = 0;
    family_req.n.nlmsg_pid = getpid();
    family_req.n.nlmsg_len = NLMSG_LENGTH(GENL_HDRLEN);
    family_req.g.cmd = CTRL_CMD_GETFAMILY;
    family_req.g.version = 0x1;
    
    na = (struct nlattr *) GENLMSG_DATA(&family_req);
    na->nla_type = CTRL_ATTR_FAMILY_NAME;
    /*------change here--------*/
    na->nla_len = strlen("CONTROL_EXMPL") + 1 + NLA_HDRLEN;

    strcpy(NLA_DATA(na), "CONTROL_EXMPL");
    
    family_req.n.nlmsg_len += NLMSG_ALIGN(na->nla_len);
    
    int rc = send_to_kernel(netlink_socket, (char *) &family_req, family_req.n.nlmsg_len);
    if ( rc < 0){
        rlog("send_to_kernel failed...");
		return -1;
    }
    
	rep_len = recv(netlink_socket, &ans, sizeof(ans), 0);
    if (rep_len < 0){
		rlog("reply length < 0");
		return -1;
	}
    
    /* Validate response message */
    if (!NLMSG_OK((&ans.n), rep_len)){
		rlog("invalid reply message");
		return -1;
	}
    
    if (ans.n.nlmsg_type == NLMSG_ERROR) { /* error */
        rlog("received error");
        return -1;
    }
    
    na = (struct nlattr *) GENLMSG_DATA(&ans);
    na = (struct nlattr *) ((char *) na + NLA_ALIGN(na->nla_len));
    if (na->nla_type == CTRL_ATTR_FAMILY_ID) {
        id = *(__u16 *) NLA_DATA(na);
    }
    return id;
}

int send_to_kernel(int netlink_socket, const char *message, int length){
    struct sockaddr_nl nladdr;
    int r;
    
    memset(&nladdr, 0, sizeof(nladdr));
    nladdr.nl_family = AF_NETLINK;
    
    while ((r = sendto(netlink_socket, message, length, 0, (struct sockaddr *) &nladdr,
                       sizeof(nladdr))) < length) {
        if (r > 0) {
            message += r;
            length -= r;
        } else if (errno != EAGAIN)
            return -1;
    }
    return 0;
}

int message_to_kernel(char* message){

    int nl_sd = create_netlink_socket(0);
    if(nl_sd < 0){
        printf("create netlink socket failure\n");
        return 0;
    }
    int id = get_family_id(nl_sd);
	struct {
        struct nlmsghdr n;
        struct genlmsghdr g;
        char buf[256];
    } ans;
    
    struct {
        struct nlmsghdr n;
        struct genlmsghdr g;
        char buf[256];
    } req;
    struct nlattr *na;
    
    /* Send command needed */
    req.n.nlmsg_len = NLMSG_LENGTH(GENL_HDRLEN);
    req.n.nlmsg_type = id;
    req.n.nlmsg_flags = NLM_F_REQUEST;
    req.n.nlmsg_seq = 60;
    req.n.nlmsg_pid = getpid();
    req.g.cmd = 1;//DOC_EXMPL_C_ECHO;
    
    /* Compose message */
    na = (struct nlattr *) GENLMSG_DATA(&req);
    na->nla_type = 1; //DOC_EXMPL_A_MSG
    int mlength = strlen(message) + 2;  // +2: \0
    na->nla_len = mlength+NLA_HDRLEN; //message length
    memcpy(NLA_DATA(na), message, mlength);
    req.n.nlmsg_len += NLMSG_ALIGN(na->nla_len);
    
    /* Send message */
	struct sockaddr_nl nladdr;
    int r;
    
    memset(&nladdr, 0, sizeof(nladdr));
    nladdr.nl_family = AF_NETLINK;
    
	r = sendto(nl_sd, (char *)&req, req.n.nlmsg_len, 0,
               (struct sockaddr *) &nladdr, sizeof(nladdr));
	
    /* Recv message */
	int rep_len = recv(nl_sd, &ans, sizeof(ans), 0);
    
    /* Validate response message */
    if (ans.n.nlmsg_type == NLMSG_ERROR) { /* error */
        rlog("error received NACK - leaving");
        return -1;
    }
    if (rep_len < 0) {
        rlog("error receiving reply message via Netlink");
        return -1;
    }
    if (!NLMSG_OK((&ans.n), rep_len)) {
        rlog("invalid reply message received via Netlink");
		return -1;
	}
    
    rep_len = GENLMSG_PAYLOAD(&ans.n);

    /* Parse reply message */
    na = (struct nlattr *) GENLMSG_DATA(&ans);
    char * result = (char *)NLA_DATA(na);
    
    close(nl_sd);    
    return(atoi(result));
}
