/*******************************************************************************************************

  CoAP GPIO Server

  TODO: add command line args to allow configuration of multiple GPIO pins, input (-i) or output (-o)
     IE:  -o 114 -0 96 -i 48



*******************************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <error.h>
#include <unistd.h>
/* #include <sqlite3.h> */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>
#include <coap/coap.h>
#include <coap/resource.h>


#define UNUSED_PARAM __attribute__ ((unused))
#define GPIO_DIR "/sys/class/gpio"

extern int gpio_export(unsigned int gpio);
extern int gpio_unexport(unsigned int gpio);
extern int gpio_set_dir(unsigned int gpio, unsigned int out_flag);
extern int gpio_set_value(unsigned int gpio, unsigned int value);
extern int gpio_get_value(unsigned int gpio, unsigned int *value);


/* gpio in/out pin defs */
uint8_t gpio_in_pins[8] = {0,0,0,0,0,0,0,0};
uint8_t gpio_out_pins[8] = {0,0,0,0,0,0,0,0};
uint8_t gpio_in_count = 0, gpio_out_count = 0;

/* temporary storage for dynamic resource representations */
static int quit = 0;
struct coap_resource_t *gpio_resource = NULL;

/* SIGINT handler: set quit to 1 for graceful termination */
static void handle_sigint(int signum UNUSED_PARAM)
{
  quit = 1;
}
static void hnd_set_gpio(coap_context_t *ctx UNUSED_PARAM,
             struct coap_resource_t *resource UNUSED_PARAM,
             const coap_endpoint_t *local_interface UNUSED_PARAM,
             coap_address_t *peer UNUSED_PARAM,
             coap_pdu_t *request,
             str *token UNUSED_PARAM,
             coap_pdu_t *response)
{
  size_t size;
  unsigned char *data;
  uint8_t val;

  resource->dirty = 1;
  coap_get_data(request, &size, &data);
    val = atoi((const char *)data);

  /* ensure data is a byte */
  response->hdr->code = COAP_RESPONSE_CODE(201);

  /*SET GPIO */
    int i;
    for( i = 0; i < gpio_out_count; i++)
    {
      int bit;
      bit = (val >> i) & 1;
      gpio_set_value(gpio_out_pins[i], bit);
    }
}

static void hnd_get_gpio(
            coap_context_t  *ctx,
            struct coap_resource_t *resource,
            const coap_endpoint_t *local_interface UNUSED_PARAM,
            coap_address_t *peer,
            coap_pdu_t *request,
            str *token,
            coap_pdu_t *response)
{
    unsigned char buf[1];
    response->hdr->code = COAP_RESPONSE_CODE(205); /* TODO: ensure GPIO is availble?? */

    if (coap_find_observer(resource, peer, token))
    {
         coap_add_option(response, COAP_OPTION_OBSERVE, coap_encode_var_bytes(buf, ctx->observe), buf);
    }

    coap_add_option(response, COAP_OPTION_CONTENT_FORMAT, coap_encode_var_bytes(buf, COAP_MEDIATYPE_TEXT_PLAIN), buf);

    /* int value;
    gpio_get_value(96, &value); */
    uint8_t values = 0;

    int i;
    for( i = 0; i < gpio_out_count; i++)
    {
      int val;
      gpio_get_value(gpio_in_pins[i], &val);

      if(i)
        values |= (val << i);
    }

    printf("GET: %d\n", values );
    fflush(stdout);
    sprintf(buf, "%d", values);
    coap_add_data(response, strlen(buf), buf);
}

static void init_resources(coap_context_t *ctx)
{
    coap_resource_t *r;
    r = coap_resource_init((unsigned char *)"gpio", 4, COAP_RESOURCE_FLAGS_NOTIFY_CON);
    coap_register_handler(r, COAP_REQUEST_GET, hnd_get_gpio);
    coap_register_handler(r, COAP_REQUEST_PUT, hnd_set_gpio);
    coap_add_resource(ctx, r);
}

static coap_context_t *get_context(const char *node, const char *port)
{
  coap_context_t *ctx = NULL;
  int s;
  struct addrinfo hints;
  struct addrinfo *result, *rp;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET6;    /* Allow IPv4 or IPv6 */
  hints.ai_socktype = SOCK_DGRAM; /* Coap uses UDP */
  hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST;

  s = getaddrinfo(node, port, &hints, &result);
  if ( s != 0 ) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
    return NULL;
  }

  /* iterate through results until success */
  for (rp = result; rp != NULL; rp = rp->ai_next)
  {
    coap_address_t addr;

    if (rp->ai_addrlen <= sizeof(addr.addr))
    {
      coap_address_init(&addr);
      addr.size = rp->ai_addrlen;
      memcpy(&addr.addr, rp->ai_addr, rp->ai_addrlen);
      ctx = coap_new_context(&addr);

      if (ctx)
      {
        /* TODO: output address:port for successful binding */
        goto finish;
      }
    }
  }

  fprintf(stderr, "no context available for interface '%s'\n", node);

  finish:
  freeaddrinfo(result);
  return ctx;
}

static int join(coap_context_t *ctx, char *group_name)
{
  struct ipv6_mreq mreq;
  struct addrinfo   *reslocal = NULL, *resmulti = NULL, hints, *ainfo;
  int result = -1;

  /* we have to resolve the link-local interface to get the interface id */
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET6;
  hints.ai_socktype = SOCK_DGRAM;

  result = getaddrinfo("::", NULL, &hints, &reslocal);
  if (result < 0) {
    fprintf(stderr, "join: cannot resolve link-local interface: %s\n",
            gai_strerror(result));
    goto finish;
  }

  /* get the first suitable interface identifier */
  for (ainfo = reslocal; ainfo != NULL; ainfo = ainfo->ai_next)
  {
    if (ainfo->ai_family == AF_INET6) {
      mreq.ipv6mr_interface =
                ((struct sockaddr_in6 *)ainfo->ai_addr)->sin6_scope_id;
      break;
    }
  }

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET6;
  hints.ai_socktype = SOCK_DGRAM;

  /* resolve the multicast group address */
  result = getaddrinfo(group_name, NULL, &hints, &resmulti);

  if (result < 0)
  {
    fprintf(stderr, "join: cannot resolve multicast address: %s\n", gai_strerror(result));
    goto finish;
  }

  for (ainfo = resmulti; ainfo != NULL; ainfo = ainfo->ai_next)
  {
    if (ainfo->ai_family == AF_INET6)
    {
      mreq.ipv6mr_multiaddr = ((struct sockaddr_in6 *)ainfo->ai_addr)->sin6_addr;
      break;
    }
  }

  result = setsockopt(ctx->sockfd, IPPROTO_IPV6, IPV6_JOIN_GROUP, (char *)&mreq, sizeof(mreq));
  if (result < 0)
    perror("join: setsockopt");

 finish:
  freeaddrinfo(resmulti);
  freeaddrinfo(reslocal);

  return result;
}

int main(int argc, char **argv)
{
  coap_context_t  *ctx;
  char addr_str[NI_MAXHOST] = "::";
  char port_str[NI_MAXSERV] = "5683";
  char *group = NULL;
  fd_set readfds;
  coap_queue_t *nextpdu;
  coap_tick_t now;
  struct timeval tv, *timeout;
  int result;

  /*parse args */
  /* char *cvalue = NULL; */
  /* int index; */
  int c, pin;

  while ((c = getopt (argc, argv, "oi:")) != -1)
  {
    switch (c)
    {
      case 'o': /*  -o86 -o23 */
        pin = atoi(optarg);
        gpio_out_pins[gpio_out_count++] = pin;
        gpio_export(pin);
        gpio_set_dir(pin, 1);
        break;

      case 'i':
        pin = atoi(optarg);
        gpio_in_pins[gpio_in_count++] = pin;
        gpio_export(pin);
        gpio_set_dir(pin, 0);
        break;
    }
  }
  /* set GPIOs */
  gpio_export(114);
  gpio_set_dir(114, 0);

  ctx = get_context(addr_str, port_str);
  if (!ctx)
    return -1;

  init_resources(ctx);

  /* TODO: how to set group variable?  */
  /* join multicast group if requested at command line */
  if (group)
  {
    join(ctx, group);
  }

  signal(SIGINT, handle_sigint);
  printf("Ready...\n");
  fflush(stdout);

  while ( !quit )
  {
    FD_ZERO(&readfds);
    FD_SET( ctx->sockfd, &readfds );
    nextpdu = coap_peek_next( ctx );
    coap_ticks(&now);

    while (nextpdu && nextpdu->t <= now - ctx->sendqueue_basetime)
    {
      coap_retransmit( ctx, coap_pop_next( ctx ) );
      nextpdu = coap_peek_next( ctx );
    }

    if ( nextpdu && nextpdu->t <= COAP_RESOURCE_CHECK_TIME )
    {
      /* set timeout if there is a pdu to send before our automatic timeout occurs */
      tv.tv_usec = ((nextpdu->t) % COAP_TICKS_PER_SECOND) * 1000000 / COAP_TICKS_PER_SECOND;
      tv.tv_sec = (nextpdu->t) / COAP_TICKS_PER_SECOND;
      timeout = &tv;
    } else
    {
      tv.tv_usec = 0;
      tv.tv_sec = COAP_RESOURCE_CHECK_TIME;
      timeout = &tv;
    }
    result = select( FD_SETSIZE, &readfds, 0, 0, timeout );

    if ( result < 0 )        /* error */
    {
      if (errno != EINTR)
        perror("select");
    }
    else if ( result > 0 )   /* read from socket */
    {
      if ( FD_ISSET( ctx->sockfd, &readfds ) )
      {
        coap_read( ctx );       /* read received data */
        /* coap_dispatch( ctx );  /\* and dispatch PDUs from receivequeue *\/ */
      }
    }
  }

  coap_free_context(ctx);
  return 0;
}
