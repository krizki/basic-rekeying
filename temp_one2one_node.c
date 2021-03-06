#include "contiki.h"
#include "contiki-net.h"
#include "net/ipv6/multicast/uip-mcast6.h"
#include "aes256.h"
#include "skipjack.c"
#include <stdio.h>
#include "net/ip/uip-debug.h"

#undef CIPHMODE
#define CIPHMODE		0 /*AES = 0, SkipJack = 1, Default HW Cipher = 2 */
#if CIPHMODE == 0
  #define KEYSIZE		32 /* In byte */
#elif CIPHMODE == 1
  #define KEYSIZE		10 /* In byte */
#endif

#define ENERG_EN		0 /* 0 or 1 */
#if ENERG_EN
  #include "sys/energest.h"
#endif

#define DEBUG			DEBUG_PRINT
#define DEBUG_LOCAL		0
#define ID_LENGTH		4 /* In byte */
#define MCAST_SINK_UDP_PORT	3001 /* Host byte order */

static uint8_t i;
static uint8_t groupKey[KEYSIZE] = {xxxx}; /* Group Key */
static uint8_t nodeKey[KEYSIZE] = {xxxx}; /* Node Key */

static struct uip_udp_conn *motes_conn;
/*---------------------------------------------------------------------------*/
PROCESS(motes_process, "Multicast Sink");
AUTOSTART_PROCESSES(&motes_process);
/*---------------------------------------------------------------------------*/
static void
PRINTARR(char* title, uint8_t* arry, uint8_t size) 
{ 
  printf(title);			
  for (i = 0; i < size; i++)		
  printf("%02x", arry[i]);		
  printf("\n");
}
/*---------------------------------------------------------------------------*/
static void
msg_dec(uint8_t* appdata, uint8_t appdataLen)
{ 
  // Initilization
  uint8_t type = appdata[0];
#if CIPHMODE == 0
  uint8_t len_inp = 48;
#elif CIPHMODE == 1
  uint8_t len_inp = 16;
#endif

  // Defining the key
  uint8_t key[KEYSIZE];

  // Decryption input and output
  uint8_t inp_dec[240];
  uint8_t out_dec[240];

  // Processing the message
  // Choosing the key
  if (type == 1) memcpy(key, groupKey, KEYSIZE * sizeof(uint8_t));
  else if (type == 2) memcpy(key, nodeKey, KEYSIZE * sizeof(uint8_t));
  else return;

  // Decryption input
  memcpy(inp_dec, &appdata[1], len_inp * sizeof(uint8_t));

  // AES CBC Decryption
#if CIPHMODE == 0
  aes256_decrypt_cbc(inp_dec, len_inp, key, out_dec);
#elif CIPHMODE == 1
  doSJDecrypt(key, inp_dec, len_inp, out_dec);
#endif

  // Display the input and output of AES CBC decryption
#if DEBUG_LOCAL
    PRINTARR("Decryption Input: ", inp_dec, len_inp);
    PRINTARR("Decryption Key: ", key, KEYSIZE);
    PRINTARR("Decryption Output: ", out_dec, len_inp);
#endif

  // Verifying Decryption resut by checking the padding
  uint8_t padlen = 0;
  padlen = (uint8_t) out_dec[(len_inp) - 1];

  // Copy the padding part of decryption result
  uint8_t padtemp1[16];
  memset(&padtemp1, 0, padlen * sizeof(uint8_t));
  memcpy(&padtemp1, &out_dec[len_inp - padlen], padlen * sizeof(uint8_t));
#if DEBUG_LOCAL
    PRINTARR("Result Padding1: ", padtemp1, padlen);
#endif

  // Generate the template for padding checking by copying the last byte of decryption result (X) X times
  uint8_t padtemp2[16];
  memset(&padtemp2, padlen, padlen * sizeof(uint8_t));
#if DEBUG_LOCAL
    PRINTARR("Result Padding2: ", padtemp2, padlen);
#endif

  int verify = memcmp(padtemp1, padtemp2, padlen * sizeof(uint8_t));
#if DEBUG_LOCAL
  if (verify == 0) printf("AES Decryption result is OK with padding length %u byte(s)\n", padlen);
  else printf("AES Decryption result is NOT OK with padding length %u byte(s)\n", padlen);
#endif

  // Calculate new Group Key
  memcpy(&groupKey, &out_dec, KEYSIZE * sizeof(uint8_t));

#if DEBUG_LOCAL
  PRINTARR("New Group Key: ", groupKey, KEYSIZE);
#endif
}
/*---------------------------------------------------------------------------*/
static void
tcpip_handler(void)
{
  uint8_t *appdata;
  uint8_t appdataLen;

  if(uip_newdata()) {
#if DEBUG_LOCAL
      PRINTARR("Buffer Data: ", uip_appdata, uip_datalen());
#endif
    if(memcmp(&appdata, &uip_appdata, uip_datalen() * sizeof(uint8_t))) {
      appdata = (uint8_t *)uip_appdata;
      appdata[uip_datalen()] = 0;
      appdataLen = uip_datalen();
      msg_dec(appdata, appdataLen);
    }
  }
}
/*---------------------------------------------------------------------------*/
static void
set_addr(void)
{
  uip_ipaddr_t addr;

  /* First, set our v6 global */
  uip_ip6addr(&addr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
  uip_ds6_set_addr_iid(&addr, &uip_lladdr);
  uip_ds6_addr_add(&addr, 0, ADDR_AUTOCONF);
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(motes_process, ev, data)
{
  PROCESS_BEGIN();
  NETSTACK_MAC.off(1);

#if ENERG_EN
  uint32_t cpu_start_time, cpu_time;
  ENERGEST_OFF(ENERGEST_TYPE_CPU);
  ENERGEST_ON(ENERGEST_TYPE_CPU);
#endif

  set_addr();
  motes_conn = udp_new(NULL, UIP_HTONS(0), NULL);
  udp_bind(motes_conn, UIP_HTONS(MCAST_SINK_UDP_PORT));

#if DEBUG_LOCAL
    PRINTARR("Current Group Key: ", groupKey, KEYSIZE);
    PRINTARR("Current Node Key: ", nodeKey, KEYSIZE);
#endif

  while(1) {
    PROCESS_YIELD();
    if(ev == tcpip_event) {
#if ENERG_EN
      cpu_start_time = energest_type_time(ENERGEST_TYPE_CPU);
#endif
      tcpip_handler();

#if ENERG_EN
      cpu_time = energest_type_time(ENERGEST_TYPE_CPU) - cpu_start_time;
      printf("Time: CPU %lu\n", cpu_time);
#endif
    }
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
