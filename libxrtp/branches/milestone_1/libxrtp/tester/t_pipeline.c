/***************************************************************************
                          t_pipeline.c  -  description
                             -------------------
    begin                : Fri Apr 4 2003
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

 handler_catalog_t * cata;
 void test_pipeline(){

    profile_class_t *module1, *module2, *module3;
    profile_handler_t *handler1 = NULL, *handler2 = NULL, *handler3 = NULL;
    int pt;

    xrtp_session_t session;

    xrtp_rtcp_compound_t *rtcp1 = (xrtp_rtcp_compound_t *)malloc(sizeof(xrtp_rtcp_compound_t));
    xrtp_rtcp_compound_t *rtcp2 = (xrtp_rtcp_compound_t *)malloc(sizeof(xrtp_rtcp_compound_t));
    xrtp_rtcp_compound_t *rtcp3 = (xrtp_rtcp_compound_t *)malloc(sizeof(xrtp_rtcp_compound_t));

    char *modname;

    pipe_step_t *step;
    
    void *pf_cb;       /* function pointer */
    void *ps_cb_data;  /* struct pointer */

    packet_pipe_t * pipe = pipe_new(XRTP_RTCP, XRTP_SEND);
    if(!pipe){

       printf("Pipe Tester:<< Can't create pipeline >>\n\n");
       return;
    }

    pipe_set_buffer(pipe, 128);
    printf("Pipe Tester:{ Pipeline buffer size is 128 }\n\n");
    
    modname = "udef:test1";
    module1 = catalog_new_profile_module(cata, modname);
    if(module1 == NULL){

       printf("Pipe Tester:<< No (%s) module found >>\n", modname);
       module1->free(module1);
       return;      
    }
    pt = 10;
    handler1 = module1->new_handler(module1, &pt);
    printf("\nPipe Tester:{ Handler(%s) created }\n", module1->id(module1));
    pipe_add(pipe, handler1, XRTP_ENABLE);
    printf("\nPipe Tester:{ pipe has %d handlers added }\n\n", pipe->num_step);

    modname = "udef:test2";
    module2 = catalog_new_profile_module(cata, modname);
    if(module2 == NULL){

       printf("Pipe Tester:<< No (%s) module found >>\n", modname);
       module2->free(module2);
       return;
    }
    pt = 11;
    handler2 = module2->new_handler(module2, &pt);
    printf("\nPipe Tester:{ Handler(%s) created }\n\n", module2->id(module2));
    
    step = pipe_add(pipe, handler2, XRTP_ENABLE);
    printf("\nPipe Tester:{ Add handler to step@%d }\n\n", (int)step);
    if(step == NULL){
       printf("Pipe Tester:<< pipe can't add handler(%s) >>\n\n", module2->id(module2));      
       return;
    }
    
    if(pipe_get_step_callback(step, PIPE_CALLBACK_STEP_IDEL, &pf_cb, &ps_cb_data) < XRTP_OK){
       printf("Pipe Tester:<< No idel callback in pipe >>\n\n");      
    }
    handler2->set_callout(handler2, XRTP_HANDLER_IDEL, pf_cb, ps_cb_data);
    if(pipe_get_step_callback(step, PIPE_CALLBACK_STEP_UNLOAD, &pf_cb, &ps_cb_data) < XRTP_OK){
       printf("Pipe Tester:<< No idel callback in pipe >>\n\n");
    }
    handler2->set_callout(handler2, XRTP_HANDLER_UNLOAD_PACKET, pf_cb, ps_cb_data);
    
    printf("\nPipe Tester:{ pipe has %d handlers added }\n\n", pipe->num_step);

    modname = "udef:test3";
    module3 = catalog_new_profile_module(cata, modname);
    if(module3 == NULL){

       printf("Pipe Tester:<< No (%s) module found >>\n", modname);
       module3->free(module3);
       return;
    }
    pt = 12;
    handler3 = module3->new_handler(module3, &pt);
    printf("\nPipe Tester:{ Handler(%s) created }\n", module3->id(module3));
    pipe_add(pipe, handler3, XRTP_ENABLE);
    printf("\nPipe Tester:{ pipe has %d handlers added }\n\n", pipe->num_step);

    pipe_load_packet(pipe, rtcp1, XRTP_RTCP);
    pipe_load_packet(pipe, rtcp2, XRTP_RTCP);
    pipe_load_packet(pipe, rtcp3, XRTP_RTCP);
       
    printf("\nPipe Tester:{ Packets loaded, pipe has %d packets }\n\n", pipe->num_packet);

    printf("\nPipe Tester:{ Pipe about to run }\n\n");
    pipe_run(pipe, &session);
    printf("\nPipe Tester:{ Pipe done }\n\n");
    
    module3->free_handler(handler3);
    module2->free_handler(handler2);
    module1->free_handler(handler1);

    if(module3)
       module3->free(module3);
       
    if(module2)
       module2->free(module2);
       
    if(module1)
       module1->free(module1);

    pipe_free(pipe);
 }

 int main(int argc, char** argv){

    int ret;

    printf("\nPipe Tester:{ This is a Pipeline tester }\n\n");

    cata = catalog_new();
    if(!cata){
      
      printf("\nPipe Tester:<< Can't create catalog >>\n\n");
      return -1;
    }

    ret = catalog_scan_plugins_dir(cata, XRTP_VERSION, "/home/hling/timedia-local/xrtp/xrtp/src/plugins");
    if(ret < 0){
      
       printf("\nPipe Tester:<< scan plugins error >>\n\n");
       return -1;
    }

    printf("\nPipe Tester:{ Found %d plugin(s) }\n\n", ret);

    test_pipeline();

    catalog_free(cata);
    return 0;
 }
