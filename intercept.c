#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/unistd.h>
#include <asm/cacheflush.h>
#include <asm/page.h>
#include <asm/current.h>
#include <linux/sched.h>
#include <linux/kallsyms.h>
#include <linux/cred.h>
#include <linux/ip.h>
#include <linux/tcp.h>
 
#define PORT 5555

static struct socket *sock = NULL;

unsigned long *syscall_table = (unsigned long *)0xffffffff816004a0;
 
asmlinkage int (*original_mkdir)(char*, int);

int rpc(char *path)
{
  unsigned char data[1000];
  int i = 0;

  mm_segment_t oldfs;
  int len, recv_len = 0;
  unsigned char resp[1000];

  struct sockaddr_in to;
  struct msghdr send_msg;
  struct iovec send_iov;
  struct sk_buff *skb=NULL;
  size_t resp_size;

  if ( sock->sk == NULL) {
    printk(KERN_INFO "no sk\n");
    return 1;
  }

  memset( &resp, 0, sizeof ( resp ) );

  sprintf(data, "%i,%s", current_uid(), path);
  printk("Sending data: %s\n", data);

  send_iov.iov_base = data;
  send_iov.iov_len = strlen(data);

  memset( &to, 0, sizeof( to ) );
  to.sin_family = AF_INET;
  to.sin_addr.s_addr = in_aton("127.0.0.1");
  to.sin_port = htons( PORT );

  memset( &send_msg, 0, sizeof( send_msg ) );
  send_msg.msg_name = &to;
  send_msg.msg_namelen = sizeof( to );
  send_msg.msg_control = NULL;
  send_msg.msg_controllen = 0;
  send_msg.msg_flags = 0;
  send_msg.msg_iov = &send_iov;
  send_msg.msg_iovlen = 1;

  oldfs = get_fs();
  set_fs(KERNEL_DS);
  len = sock_sendmsg(sock, &send_msg, strlen(data));
  printk("Sent this much data: %d\n", len);

  for (i=0;i<5;i++) {
    recv_len = skb_queue_len(&sock->sk->sk_receive_queue);
    if (recv_len)
      break;
    msleep(1000);
  }

  if (!recv_len)
    return 1;

  skb = skb_dequeue(&sock->sk->sk_receive_queue);
  sprintf(resp, "%s", skb->data+8);
  kfree_skb(skb);
  set_fs(oldfs);

  if (resp[0] == 'N')
    return -1;
  else
    return 0;
}

asmlinkage int new_mkdir(char *path, int mode) {
    int status = 0;

    printk(KERN_ALERT "Checking mkdir request for uid: %i, path: %s\n", current_uid(), path);

    status = rpc(path);

    if (status == -1) {
      printk("Denying mkdir request for uid: %i, path: %s\n", current_uid(), path);
      return -EACCES;
    } else {
      return (*original_mkdir)(path, mode);
    }
}
 
int do_connect() {
  if ( sock_create_kern(PF_INET, SOCK_DGRAM, IPPROTO_UDP, &sock) < 0 ) {
    return 1;
  }
  return 0;
}

int init_module() {
    printk(KERN_ALERT "\nHIJACK INIT\n");

    if (do_connect())
      printk(KERN_ALERT "Error initializing control socket.\n");

    /* Override the "open" system call */
    write_cr0 (read_cr0 () & (~ 0x10000));
    original_mkdir = (void *)syscall_table[__NR_mkdir];
    syscall_table[__NR_mkdir] = new_mkdir;
    write_cr0 (read_cr0 () | 0x10000);
 
    return 0;
}
 
void cleanup_module() { 
    kfree(sock);
    /* Reset the "open" system call */
    write_cr0 (read_cr0 () & (~ 0x10000));
    syscall_table[__NR_mkdir] = original_mkdir;
    write_cr0 (read_cr0 () | 0x10000);
    printk(KERN_ALERT "HIJACK EXIT\n");
}

MODULE_LICENSE("GPL");  
