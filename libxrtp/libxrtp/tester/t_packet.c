/***************************************************************************
                          t_packet.c  -  description
                             -------------------
    begin                : Wed Apr 30 2003
    copyright            : (C) 2003 by Heming Ling
    email                : 
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

 #include "../internal.h"
 #include "../const.h"

 void buffer_test(){

    printf("[Buffer Testing start]\n");

    uint16 vin = 1024, vout;
    uint16 win = 54321, wout;
    uint16 xin = 12345, xout;
    uint32 yin = 256, yout;

    xrtp_buffer_t * buf = buffer_new(10, NET_ORDER);

    buffer_add_uint16(buf, vin);
    buffer_add_uint16(buf, win);
    buffer_add_uint16(buf, xin);
    buffer_add_uint32(buf, yin);
    buffer_seek(buf, 0);

    buffer_next_uint16(buf, &vout);
    buffer_next_uint16(buf, &wout);
    buffer_next_uint16(buf, &xout);
    buffer_next_uint32(buf, &yout);
    
    printf("host order is %d, buffer order is %d\n", HOST_BYTEORDER, buf->byte_order);
    printf("vin = %d, vout = %d\n", vin, vout);
    printf("win = %d, wout = %d\n", win, wout);
    printf("xin = %d, xout = %d\n", xin, xout);
    printf("yin = %d, yout = %d\n", yin, yout);
    
    printf("[Buffer Testing end]\n\n");
 }

 xrtp_session_t * ses = NULL;

 void rtcp_test(){

    printf("\nPacket Tester:{ Testing RTCP Packet }\n\n");

    int npack = 5;
    int pad = 1;

    uint32 ssrc = 23456;
    xrtp_rtcp_compound_t * rtcp = rtcp_sender_report_new(ses, npack, pad);
    
    /* Set Sender info */
    uint32 hi_ntp = 456;
    uint32 lo_ntp = 678;
    uint32 rtp_ts = 12345;
    uint32 pack_sent = 19;
    uint32 oct_sent = 300;
    rtcp_set_sender_info(rtcp, ssrc, hi_ntp, lo_ntp, rtp_ts, pack_sent, oct_sent);
    
     
    /* Add report */
    ssrc = 65432;
    uint8 frac_lost = 23; /* fix-point float */
    uint32 total_lost = 50;  /* 24 bits */
    uint32 seqn = 6789;
    uint32 jitter = 334;
    uint32 lsr_ts = 123456;
    uint32 delay = 321;
    rtcp_add_report(rtcp, ssrc, frac_lost, total_lost, seqn,
                    jitter, lsr_ts, delay);

    /* Add SDES */
    uint src = 23456;
    char * cname = xstr_new_string("xrtp");
    rtcp_add_sdes(rtcp,  src, RTCP_SDES_CNAME, strlen(cname)+1, cname);
    char * name = xstr_new_string("extendable rtp library");
    rtcp_add_sdes(rtcp,  src, RTCP_SDES_NAME, strlen(name)+1, name);
    rtcp_end_sdes(rtcp,  src);

    src = 34567;      
    char * prev = xstr_new_string("my");
    char * priv = xstr_new_string("sdes");
    rtcp_add_priv_sdes(rtcp, src, strlen(prev)+1, prev, strlen(priv)+1, priv);
    rtcp_end_sdes(rtcp,  src);

    /* Add APP */
    uint8 stype = 16;
    uint32 nm = 65536;
    char * app = xstr_new_string("appdata");
    rtcp_new_app(rtcp, src, stype, nm, strlen(app)+1, app);  /* len MUST be 4x */

    /* Add BYE */
    uint32 srcs[4] = {2345, 3456, 5678, 6789};
    char * why = xstr_new_string("That's why");
    rtcp_new_bye(rtcp, srcs, 4, strlen(why)+1, why);

    printf("<<<<<< RTCP before send >>>>>>\n\n");
    
    /* Show RTCP */
    rtcp_show(rtcp);

    xrtp_buffer_t * comp = rtcp_pack(rtcp);
    if(!comp){

       printf("Error when pack a rtcp\n");
       
    }else{

       printf("\nrtcp pack in %d bytes\n", buffer_datalen(comp));
    }

    /* Unpack the packet */
    xrtp_rtcp_compound_t * rtcp2 = rtcp_new_compound(ses, npack, XRTP_RECEIVE, NULL, 0, 0);
    if(!rtcp2){

      printf("\nPacket Tester:<< Can't create rtcp compound packet for recieving >>\n\n");
      return;
    }

    xrtp_buffer_t * pack2 = buffer_clone(comp);

    int ret = rtcp_set_buffer(rtcp2, pack2);
    ret = rtcp_unpack(rtcp2);
    if(ret < XRTP_OK){

       printf("\nPacket Tester:<< ERROR No.%d >>\n\n", ret);
       return;
    }

    printf("<<<<<< Send to receiver >>>>>>\n\n");

    /* Show RTCP */
    rtcp_show(rtcp2);

    /* Release RTP Packet */
    rtcp_compound_done(rtcp);
    rtcp_compound_done(rtcp2);
 }

 void rtp_test(){
    
    xrtp_rtp_packet_t * rtp;
    
    printf("\nPacket Tester:{ Testing RTP Packet }\n\n");

    uint8 pt = 10;
    
    /* New RTP Packet */
    rtp = rtp_new_packet(ses, pt, XRTP_SEND, NULL, 0, 0);
    if(!rtp){

      printf("\nPacket Tester:<< Can't create rtp packet for sending >>\n\n");
      return;
    }

    /* RTP Fixed head */
    uint32 ssrc = 123456;
    uint16 seqno = 54321;   /* maximum 65536 */
    uint32 ts = 12345678;

    rtp_packet_set_head(rtp, ssrc, seqno, ts);
    rtp_packet_set_mark(rtp, 1);
    
    rtp_packet_add_csrc(rtp, 23456);
    rtp_packet_add_csrc(rtp, 34567);
    rtp_packet_add_csrc(rtp, 45678);
    rtp_packet_add_csrc(rtp, 56789);
    
    /* RTP Head Extension */
    uint16 xhdinf = 0xfe;
    char str1[] = "[RTP Head Extension Data]";
    char * xhd = xstr_new_string(str1);
    rtp_packet_set_headext(rtp, xhdinf, strlen(xhd)+1, xhd);

    /* RTP Payload */
    char str2[] = "[RTP Payload Data]";
    char * pay = xstr_new_string(str2);
    rtp_packet_set_payload(rtp, strlen(pay)+1, pay);

    printf("<<<<<< RTP before send >>>>>>\n\n");

    rtp_packet_show(rtp);

    /* Pack the packet */
    xrtp_buffer_t * pack;
    pack = rtp_pack(rtp);
    if(!pack){

       printf("Error when pack a rtp\n");
    }else{

       printf("rtp pack in %d bytes\n\n", buffer_datalen(pack));
    }

    /* Unpack the packet */
    xrtp_rtp_packet_t * rtp2 = rtp_new_packet(ses, 0, XRTP_RECEIVE, NULL, 0, 0);
    if(!rtp2){

      printf("\nPacket Tester:<< Can't create rtp packet for recieving >>\n\n");
      return;
    }

    xrtp_buffer_t * pack2 = buffer_clone(pack);

    int ret = rtp_packet_set_buffer(rtp2, pack2);
    ret = rtp_unpack(rtp2);
    if(ret != XRTP_OK){

       printf("\nPacket Tester:<< ERROR No.%d >>\n\n", ret);
       return;
    }

    printf("<<<<<< RTP on receiver >>>>>>\n\n");

    rtp_packet_show(rtp2);

    /* Release RTP Packet */
    rtp_packet_done(rtp);
    rtp_packet_done(rtp2);
 }
 
 int main(int argc, char** argv){

    printf("\nPacket Tester:{ This is a RTP Packet tester }\n\n");

    /* Buffer Tester */
    /* buffer_test(); */

    rtp_test();
    rtcp_test();

    return 0;
 }
