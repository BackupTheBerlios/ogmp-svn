/*
  The mediastreamer library aims at providing modular media processing and I/O
	for linphone, but also for any telephony application.
  Copyright (C) 2001  Simon MORLAT simon.morlat@linphone.org
  										
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "dev_alsa.h"

#include <signal.h>

#include <timedia/xmalloc.h>
#include <timedia/xstring.h>

/*
#define ALSA_LOG
*/
#define ALSA_DEBUG

#ifdef ALSA_LOG
 #define alsa_log(fmtargs)  do{ui_print_log fmtargs;}while(0)
#else
 #define alsa_log(fmtargs)
#endif

#ifdef ALSA_DEBUG
 #define alsa_debug(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define alsa_debug(fmtargs)
#endif

#define DEFAULT_OUTPUT_NSAMPLE_PULSE 1024
#define DEFAULT_INPUT_NSAMPLE_PULSE  1024

#define ALSA_INBUF_N 1
#define ALSA_CARD_NUMBER 3

static char *over_pcmdev=NULL;

void snd_card_init(SndCard *obj)
{
	memset(obj,0,sizeof(SndCard));
}

void snd_card_uninit(SndCard *obj)
{
	if (obj->card_name!=NULL)
      xfree(obj->card_name);
}

int __alsa_card_read(AlsaCard *obj,char *buf,int bsize);
int __alsa_card_write(AlsaCard *obj,char *buf,int size);

int alsa_set_params(AlsaCard *obj, int rw, int bits, int stereo, int rate);

int alsa_card_open_r(AlsaCard *obj,int bits,int channels,int rate)
{
	int bsize;
	int err;
	snd_pcm_t *pcm_handle;
	char *pcmdev;
   
	if (over_pcmdev!=NULL)
      pcmdev=over_pcmdev;
	else
      pcmdev=obj->pcmdev;

	//if (snd_pcm_open(&pcm_handle, pcmdev,SND_PCM_STREAM_CAPTURE,SND_PCM_NONBLOCK) < 0)
	if (snd_pcm_open(&pcm_handle, pcmdev,SND_PCM_STREAM_CAPTURE,0) < 0)
   {
		printf ("alsa_card_open_r: Error opening PCM device %s\n", obj->pcmdev);
		return -1;
	}
   
	if(pcm_handle==NULL) return -1;
   
	obj->read_handle=pcm_handle;
	if ((bsize=alsa_set_params(obj,0,bits,channels,rate))<0)
   {
		snd_pcm_close(pcm_handle);
		obj->read_handle=NULL;
		return -1;
	}
   
	obj->readbuf = xmalloc(bsize);

	err=snd_pcm_start(obj->read_handle);
	if (err<0)
		printf("Cannot start read pcm: %s", snd_strerror(err));

   obj->readpos=0;
   
	SND_CARD(obj)->bsize=bsize;
	SND_CARD(obj)->flags|=SND_CARD_FLAGS_OPENED;
   
	return 0;
}

int alsa_set_params(AlsaCard *obj, int rw, int bits, int channels, int rate)
{
	snd_pcm_hw_params_t *hwparams=NULL;
	snd_pcm_sw_params_t *swparams=NULL;
	snd_pcm_t *pcm_handle;
   
   unsigned int exact_rate;
	int dir;
   
	int periods = 8;
	int periodsize = 256;
   unsigned int exact_periods;
	snd_pcm_uframes_t exact_periodsize;
   
	int err;
	int format;   
   
	if (rw)
		pcm_handle=obj->write_handle;
	else
      pcm_handle=obj->read_handle;
	
	/* Allocate the snd_pcm_hw_params_t structure on the stack. */
   snd_pcm_hw_params_alloca(&hwparams);
	
	/* Init hwparams with full configuration space */
   if (snd_pcm_hw_params_any(pcm_handle, hwparams) < 0)
   {
		printf ("alsa_set_params: Cannot configure this PCM device.\n");
		return(-1);
   }
	
	if (snd_pcm_hw_params_set_access(pcm_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED) < 0)
   {
      printf ("alsa_set_params: Error setting access.\n");
      return(-1);
   }
   
	/* Set sample format */
#ifdef WORDS_BIGENDIAN
	format = SND_PCM_FORMAT_S16_BE;
#else
	format = SND_PCM_FORMAT_S16_LE;
#endif

   if (snd_pcm_hw_params_set_format (pcm_handle, hwparams, format) < 0)
   {
      printf("alsa_set_params: Error setting format.\n");
      return(-1);
   }
    
	/* Set number of channels */
   if (snd_pcm_hw_params_set_channels (pcm_handle, hwparams, channels) < 0)
   {
      printf ("alsa_set_params: Error setting channels.\n");
      return(-1);
   }
   
	/* Set sample rate. If the exact rate is not supported */
   /* by the hardware, use nearest possible rate.         */ 
	exact_rate = rate;
	dir=0;

   err=snd_pcm_hw_params_set_rate_near(pcm_handle, hwparams, &exact_rate, &dir);
   if (err<0)
   {
		printf ("alsa_set_params: Error setting rate to %i:%s", rate, snd_strerror(err));
 		return -1;
	}
   
   if (dir != 0)
   {
      printf("alsa_set_params: The rate %d Hz is not supported by your hardware.\n "
		"==> Using %d Hz instead.\n", rate, exact_rate);
   }
   
	/* choose greater period size when rate is high */
	periodsize = periodsize * (rate/8000);	
	
	/* set period size */
	exact_periodsize = periodsize;
	dir=0;
   
   if (snd_pcm_hw_params_set_period_size_near(pcm_handle, hwparams, &exact_periodsize, &dir) < 0)
   {
      printf ("alsa_set_params: Error setting period size.\n");
      return(-1);
   }
   
	if (dir != 0)
   {
      printf ("alsa_set_params: The period size %d is not supported by your hardware.\n "
		"==> Using %ld instead.\n", periodsize, exact_periodsize);
   }
   
	periodsize=exact_periodsize;
   
	/* Set number of periods. Periods used to be called fragments. */ 
	exact_periods = periods;
	dir=0;
   
   if (snd_pcm_hw_params_set_periods_near(pcm_handle, hwparams, &exact_periods, &dir) < 0)
   {
      printf ("alsa_set_params: Error setting periods.\n");
      return(-1);
   }
   
	if (dir != 0)
   {
      printf ("alsa_set_params: The number of periods %d is not supported by your hardware.\n "
		"==> Using %d instead.\n", periods, exact_periods);
   }
   
	/* Apply HW parameter settings to */
   /* PCM device and prepare device  */
   if ((err=snd_pcm_hw_params(pcm_handle, hwparams)) < 0)
   {
      printf ("alsa_set_params: Error setting HW params:%s",snd_strerror(err));
      return(-1);
   }
   
	/*prepare sw params */
	if (rw)
   {
		snd_pcm_sw_params_alloca(&swparams);
		snd_pcm_sw_params_current(pcm_handle, swparams);
      
		if ((err=snd_pcm_sw_params_set_start_threshold(pcm_handle, swparams,periodsize*2 ))<0)
      {
			printf ("alsa_set_params: Error setting start threshold:%s",snd_strerror(err));
			return -1;
		}
      
		if ((err=snd_pcm_sw_params(pcm_handle, swparams))<0)
      {
			printf ("alsa_set_params: Error setting SW params:%s",snd_strerror(err));
			return(-1);
		}
	}
   
	obj->frame_size = channels*(bits/8);
	SND_CARD(obj)->bsize = periodsize*obj->frame_size;
   
	obj->frames = periodsize;
   
	printf ("alsa_set_params:  blocksize=%i.",SND_CARD(obj)->bsize);
   
	return SND_CARD(obj)->bsize;	
}

int alsa_card_open_w(AlsaCard *obj,int bits,int channels,int rate)
{
   int bsize;
	snd_pcm_t *pcm_handle;
	char *pcmdev;
   
	if (over_pcmdev!=NULL)
      pcmdev=over_pcmdev;
	else
      pcmdev=obj->pcmdev;
	
	//if (snd_pcm_open(&pcm_handle, pcmdev,SND_PCM_STREAM_PLAYBACK,SND_PCM_NONBLOCK) < 0)
	if (snd_pcm_open(&pcm_handle, pcmdev,SND_PCM_STREAM_PLAYBACK,0) < 0)
   {
      printf ("alsa_card_open_w: Error opening PCM device %s\n", obj->pcmdev);
      return -1;
   }
   
	obj->write_handle=pcm_handle;
	if ((bsize=alsa_set_params(obj,1,bits,channels,rate))<0)
   {
		snd_pcm_close(pcm_handle);
		obj->write_handle=NULL;
		return -1;
	}
   
	obj->writebuf=xmalloc(bsize);
	
	obj->writepos=0;
	SND_CARD(obj)->bsize=bsize;
	SND_CARD(obj)->flags|=SND_CARD_FLAGS_OPENED;
	return 0;
}

void alsa_card_set_blocking_mode (AlsaCard *obj, int bool)
{
	if (obj->read_handle != NULL)
      snd_pcm_nonblock(obj->read_handle, !bool);
      
	if (obj->write_handle != NULL)
      snd_pcm_nonblock(obj->write_handle, !bool);
}

void alsa_card_close_r (AlsaCard *obj)
{
	if (obj->read_handle!=NULL)
   {
		snd_pcm_close(obj->read_handle);
		obj->read_handle=NULL;
      
		xfree(obj->readbuf);
      
		obj->readbuf=NULL;
	}
}

void alsa_card_close_w (AlsaCard *obj)
{
	if (obj->write_handle!=NULL)
   {
		snd_pcm_close(obj->write_handle);
		obj->write_handle=NULL;
      
		xfree(obj->writebuf);
		obj->writebuf=NULL;
	}
}

int alsa_card_probe(AlsaCard *obj,int bits,int stereo,int rate)
{
	int ret;
	ret=alsa_card_open_w(obj,bits,stereo,rate);
	if (ret<0) return -1;
	ret=SND_CARD(obj)->bsize;
	alsa_card_close_w(obj);
	return ret;
}

void alsa_card_destroy(AlsaCard *obj)
{
	snd_card_uninit(SND_CARD(obj));
   
	xfree(obj->pcmdev);
   
	if (obj->readbuf!=0)
      xfree(obj->readbuf);
      
	if (obj->writebuf!=0)
      xfree(obj->writebuf);	
}

int alsa_card_can_read (AlsaCard *obj)
{
	int frames;
   
	if(!obj->read_handle) return 0;
	if (obj->readpos!=0) return 1;
   
   frames=snd_pcm_avail_update(obj->read_handle);
   
   if (frames >= obj->frames) return 1;
   
	return 0;
}

int __alsa_card_read(AlsaCard *obj,char *buf,int bsize)
{
	int err;
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set,SIGALRM);
	sigprocmask(SIG_BLOCK,&set,NULL);
   
	err=snd_pcm_readi(obj->read_handle,buf,bsize/obj->frame_size);
	if (err<0)
   {
		if (err!=-EPIPE)
			printf("alsa_card_read: snd_pcm_readi() failed:%s.",snd_strerror(err));
      
		snd_pcm_prepare(obj->read_handle);
		err=snd_pcm_readi(obj->read_handle,buf,bsize/obj->frame_size);
      
		if (err<0)
         printf("alsa_card_read: snd_pcm_readi() failed:%s.",snd_strerror(err));
	}
   
	sigprocmask(SIG_UNBLOCK,&set,NULL);
   
	return err*obj->frame_size;
}

int alsa_card_read (AlsaCard *obj, char *buf, int size)
{
	int err;
	int bsize=SND_CARD(obj)->bsize;
   
	if(obj->read_handle == NULL) return -1;
   
	if (size<bsize)
   {
		int canread = bsize-obj->readpos < size ? bsize-obj->readpos : size;
		
		if (obj->readpos==0)
      {
			err=__alsa_card_read(obj,obj->readbuf,bsize);
		}
			
		memcpy(buf,&obj->readbuf[obj->readpos],canread);
      
		obj->readpos+=canread;
      
		if (obj->readpos>=bsize) obj->readpos=0;
      
		return canread;
	}
   else
   {
		err=__alsa_card_read(obj,buf,size);
		return err;
	}
}

int __alsa_card_write(AlsaCard *obj,char *buf,int size)
{
	int err;
   
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set,SIGALRM);
	sigprocmask(SIG_BLOCK,&set,NULL);
   
	if ((err=snd_pcm_writei(obj->write_handle,buf,size/obj->frame_size))<0)
   {
		if (err!=-EPIPE)
      {
			printf("alsa_card_write: snd_pcm_writei() failed:%s.",snd_strerror(err));
		}
      
		snd_pcm_prepare(obj->write_handle);
		err=snd_pcm_writei(obj->write_handle,buf,size/obj->frame_size);
      
		if (err<0) printf("alsa_card_write: Error writing sound buffer (size=%i):%s",size,snd_strerror(err));
		
	}
   
	sigprocmask(SIG_UNBLOCK,&set,NULL);
   
	return err;
}

int alsa_card_write (AlsaCard *obj, char *buf, int size)
{
	int err;
	int bsize=SND_CARD(obj)->bsize;
   
	if(obj->write_handle==NULL) return -1;
   
	if (size<bsize)
   {
		int canwrite;
		
		canwrite = bsize-obj->writepos < size ? bsize-obj->writepos : size;
      
		memcpy(&obj->writebuf[obj->writepos],buf,canwrite);
      
		obj->writepos+=canwrite;
      
		if (obj->writepos>=bsize)
      {
			err=__alsa_card_write(obj,obj->writebuf,bsize);
			obj->writepos=0;
		}
      
		return canwrite;
	}
   else
		return __alsa_card_write(obj,buf,bsize);
}

snd_mixer_t *alsa_mixer_open(AlsaCard *obj)
{
	snd_mixer_t *mixer=NULL;
	int err;
	err=snd_mixer_open(&mixer,0);
	if (err<0)
   {
		printf ("Could not open alsa mixer: %s",snd_strerror(err));
		return NULL;
	}
   
	if ((err = snd_mixer_attach (mixer, obj->mixdev)) < 0)
   {
		printf("Could not attach mixer to card: %s",snd_strerror(err));
		snd_mixer_close(mixer);
		return NULL;
	}
   
	if ((err = snd_mixer_selem_register (mixer, NULL, NULL)) < 0)
   {
		printf("snd_mixer_selem_register: %s",snd_strerror(err));
		snd_mixer_close(mixer);
		return NULL;
	}
	if ((err = snd_mixer_load (mixer)) < 0)
   {
		printf("snd_mixer_load: %s",snd_strerror(err));
		snd_mixer_close(mixer);
		return NULL;
	}
   
	obj->mixer=mixer;
   
	return mixer;
}

void alsa_mixer_close(AlsaCard *obj)
{
	snd_mixer_close(obj->mixer);
	obj->mixer=NULL;
}

typedef enum {CAPTURE, PLAYBACK, CAPTURE_SWITCH, PLAYBACK_SWITCH} MixerAction;

static int get_mixer_element(snd_mixer_t *mixer,const char *name, MixerAction action)
{
	long value=0;
	const char *elemname;
	snd_mixer_elem_t *elem;
	int err;
	long sndMixerPMin;
	long sndMixerPMax;
	long newvol;
   
	elem=snd_mixer_first_elem(mixer);
   
	while (elem!=NULL)
   {
		elemname=snd_mixer_selem_get_name(elem);
      
		//g_message("Found alsa mixer element %s.",elemname);
		if (strcmp(elemname,name)==0)
      {
			switch (action)
         {
				case CAPTURE:
				if (snd_mixer_selem_has_capture_volume(elem))
            {
					snd_mixer_selem_get_playback_volume_range(elem, &sndMixerPMin, &sndMixerPMax);
					err=snd_mixer_selem_get_capture_volume(elem,SND_MIXER_SCHN_UNKNOWN,&newvol);
					newvol-=sndMixerPMin;
					value=(100*newvol)/(sndMixerPMax-sndMixerPMin);

               if (err<0) printf("Could not get capture volume for %s:%s",name,snd_strerror(err));
               //else g_message("Succesfully get capture level for %s.",elemname);

               break;
				}
				break;
            
				case PLAYBACK:
				if (snd_mixer_selem_has_playback_volume(elem))
            {
					snd_mixer_selem_get_playback_volume_range(elem, &sndMixerPMin, &sndMixerPMax);
					err=snd_mixer_selem_get_playback_volume(elem,SND_MIXER_SCHN_FRONT_LEFT,&newvol);
					newvol-=sndMixerPMin;
					value=(100*newvol)/(sndMixerPMax-sndMixerPMin);
               
					if (err<0) printf("Could not get playback volume for %s:%s",name,snd_strerror(err));
               //else g_message("Succesfully get playback level for %s.",elemname);
               
					break;
				}
				break;
            
            case CAPTURE_SWITCH:
				break;

            case PLAYBACK_SWITCH:
				break;
			}
		}
      
		elem=snd_mixer_elem_next(elem);
	}
	
	return value;
}

static void set_mixer_element(snd_mixer_t *mixer,const char *name, int level,MixerAction action)
{
	const char *elemname;
	snd_mixer_elem_t *elem;

   long sndMixerPMin;
	long sndMixerPMax;
	long newvol;
	
	elem=snd_mixer_first_elem(mixer);
	
	while (elem!=NULL)
   {
		elemname=snd_mixer_selem_get_name(elem);
		//g_message("Found alsa mixer element %s.",elemname);
		if (strcmp(elemname,name)==0)
      {
			switch(action)
         {
				case CAPTURE:
				if (snd_mixer_selem_has_capture_volume(elem)){
					snd_mixer_selem_get_playback_volume_range(elem, &sndMixerPMin, &sndMixerPMax);
					newvol=(((sndMixerPMax-sndMixerPMin)*level)/100)+sndMixerPMin;
					snd_mixer_selem_set_capture_volume_all(elem,newvol);
					//g_message("Succesfully set capture level for %s.",elemname);
					return;
				}
				break;
				case PLAYBACK:
				if (snd_mixer_selem_has_playback_volume(elem)){
					snd_mixer_selem_get_playback_volume_range(elem, &sndMixerPMin, &sndMixerPMax);
					newvol=(((sndMixerPMax-sndMixerPMin)*level)/100)+sndMixerPMin;
					snd_mixer_selem_set_playback_volume_all(elem,newvol);
					//g_message("Succesfully set playback level for %s.",elemname);
					return;
				}
				break;
				case CAPTURE_SWITCH:
				if (snd_mixer_selem_has_capture_switch(elem)){
					snd_mixer_selem_set_capture_switch_all(elem,level);
					//g_message("Succesfully set capture switch for %s.",elemname);
				}
				break;
				case PLAYBACK_SWITCH:
				if (snd_mixer_selem_has_playback_switch(elem)){
					snd_mixer_selem_set_playback_switch_all(elem,level);
					//g_message("Succesfully set capture switch for %s.",elemname);
				}
				break;
			}
		}
      
		elem=snd_mixer_elem_next(elem);
	}

	return ;
}

void alsa_card_set_level(AlsaCard *obj, int way, int a)
{	
	snd_mixer_t *mixer;
   
	mixer=alsa_mixer_open(obj);
   
	if (mixer==NULL) return;
   
	switch(way)
   {
		case SND_CARD_LEVEL_GENERAL:
			set_mixer_element(mixer,"Master",a,PLAYBACK);
		break;
      
		case SND_CARD_LEVEL_INPUT:
			set_mixer_element(mixer,"Capture",a,CAPTURE);
		break;
      
		case SND_CARD_LEVEL_OUTPUT:
			set_mixer_element(mixer,"PCM",a,PLAYBACK);
		break;
      
		default:
			printf("oss_card_set_level: unsupported command.");
	}
   
	alsa_mixer_close(obj);
}

int alsa_card_get_level(AlsaCard *obj, int way)
{
	snd_mixer_t *mixer;
	int value;
	mixer=alsa_mixer_open(obj);
   
	if (mixer==NULL) return 0;
   
	switch(way)
   {
		case SND_CARD_LEVEL_GENERAL:
			value=get_mixer_element(mixer,"Master",PLAYBACK);
		break;
      
		case SND_CARD_LEVEL_INPUT:
			value=get_mixer_element(mixer,"Capture",CAPTURE);
		break;
      
		case SND_CARD_LEVEL_OUTPUT:
			value=get_mixer_element(mixer,"PCM",PLAYBACK);
		break;
      
		default:
			printf("oss_card_set_level: unsupported command.");
	}
   
	alsa_mixer_close(obj);
   
	return value;
}

void alsa_card_set_source(AlsaCard *obj,int source)
{
	snd_mixer_t *mixer;
	mixer=alsa_mixer_open(obj);
	if (mixer==NULL) return;
	switch (source)
   {
		case 'm':
			set_mixer_element(mixer,"Mic",1,CAPTURE_SWITCH);
			set_mixer_element(mixer,"Capture",1,CAPTURE_SWITCH);
			break;
         
		case 'l':
			set_mixer_element(mixer,"Line",1,CAPTURE_SWITCH);
			set_mixer_element(mixer,"Capture",1,CAPTURE_SWITCH);
			break;
	}
}

#if 0
MSFilter *alsa_card_create_read_filter(AlsaCard *card)
{
	MSFilter *f=ms_oss_read_new();
   
	ms_oss_read_set_device(MS_OSS_READ(f),SND_CARD(card)->index);
   
	return f;
}

MSFilter *alsa_card_create_write_filter(AlsaCard *card)
{
	MSFilter *f=ms_oss_write_new();
   
	ms_oss_write_set_device(MS_OSS_WRITE(f),SND_CARD(card)->index);
   
	return f;
}
#endif

SndCard * alsa_card_new (int devid)
{
	AlsaCard *obj;
   
	SndCard *base;
	int err;
	char *name=NULL;
	
	/* carefull: this is an alsalib call despite its name! */
	err=snd_card_get_name(devid,&name);
	if (err<0)
		return NULL;

   obj= xmalloc (sizeof(AlsaCard));
   
	base= SND_CARD(obj);
	snd_card_init(base);
	
	base->card_name = xstr_clone(name);
   
	base->_probe=(SndCardOpenFunc)alsa_card_probe;
	base->_open_r=(SndCardOpenFunc)alsa_card_open_r;
	base->_open_w=(SndCardOpenFunc)alsa_card_open_w;
	base->_can_read=(SndCardPollFunc)alsa_card_can_read;
	base->_set_blocking_mode=(SndCardSetBlockingModeFunc)alsa_card_set_blocking_mode;
	base->_read=(SndCardIOFunc)alsa_card_read;
	base->_write=(SndCardIOFunc)alsa_card_write;
	base->_close_r=(SndCardCloseFunc)alsa_card_close_r;
	base->_close_w=(SndCardCloseFunc)alsa_card_close_w;
	base->_set_rec_source=(SndCardMixerSetRecSourceFunc)alsa_card_set_source;
	base->_set_level=(SndCardMixerSetLevelFunc)alsa_card_set_level;
	base->_get_level=(SndCardMixerGetLevelFunc)alsa_card_get_level;
	base->_destroy=(SndCardDestroyFunc)alsa_card_destroy;
   /*
	base->_create_read_filter=(SndCardCreateFilterFunc)alsa_card_create_read_filter;
	base->_create_write_filter=(SndCardCreateFilterFunc)alsa_card_create_write_filter;
	*/
   {
      char pcmdev[32];
      char mixdev[32];
      
      sprintf(pcmdev, "plughw:%i,0",devid);
      sprintf(mixdev, "hw:%i",devid);
      
	   obj->pcmdev = xstr_clone(pcmdev);
	   obj->mixdev = xstr_clone(mixdev);
   }
   
	obj->readbuf=NULL;
	obj->writebuf=NULL;
   
	return base;
}

int alsa_card_manager_init (SndCardManager *m, int index)
{
	int devindex;

   int found=0;
	char *name=NULL;
   
	for(devindex=0;index<MAX_SND_CARDS && devindex<MAX_SND_CARDS ;devindex++)
   {
		if (snd_card_get_name(devindex,&name) == 0)
      {
			printf ("Found ALSA device: %s",name);
			m->cards[index]=alsa_card_new(devindex);
			m->cards[index]->index=index;
         
			found++;
			index++;
		}
	}
   
	return found;
}

void alsa_card_manager_set_default_pcm_device (char *pcmdev)
{
	if (over_pcmdev!=NULL)
		xfree(over_pcmdev);	

   over_pcmdev = xstr_clone (pcmdev);
}

/***************************************************************************/
int alsa_io_loop(void *gen)
{
	int rc;
	int sample_bytes;
   
	int64 stamp=0;
   int tick=0;

   media_frame_t auf;
	alsa_device_t* alsa = (alsa_device_t*)gen;
   short *pcm_w;
   
   int npcm_need;

   char *outbuf = xmalloc(alsa->output_nbyte_once);
   if(!outbuf)
   {
      return MP_FAIL;
   }
   
	sample_bytes = alsa->ai_input->info.sample_bits / OS_BYTE_BITS;
	alsa_log(("alsa_input_loop: ALSA started, %d bytes/sample\n\n", sample_bytes));

   alsa->npcm_input = 0;
   
	alsa->input_stop = 0;
	while(!alsa->input_stop)
	{
      if(alsa->src_pipe)
      {
         int npcm_output = alsa->src_pipe->pick_content(alsa->src_pipe, (media_info_t*)alsa->ai_output, outbuf, alsa->output_nbyte_once);

         if(npcm_output <= 0)
         {
	         alsa_debug (("\ralsa_io_loop: output underrun\n"));
            memset(outbuf, 0, alsa->output_npcm_once/2);
            npcm_output = alsa->output_npcm_once/2;
         }
         
         rc = snd_pcm_writei(((AlsaCard*)alsa->sndcard)->write_handle, outbuf, npcm_output);
      }
      
      if(alsa->receiver)
      {
         pcm_w = (short*)&alsa->pcm_input[alsa->npcm_input];
         npcm_need = alsa->input_npcm_once - alsa->npcm_input;
      
         rc = snd_pcm_readi(((AlsaCard*)alsa->sndcard)->read_handle, pcm_w, npcm_need);
         if (rc == -EPIPE)
         {
            /* EPIPE means overrun */
            fprintf(stderr, "overrun occurred\n");
            snd_pcm_prepare(((AlsaCard*)alsa->sndcard)->read_handle);
         }
         else if (rc < 0)
         {
            fprintf(stderr, "error from read: %s\n", snd_strerror(rc));
         }
         else if (rc != (int)alsa->input_npcm_once)
         {
               fprintf(stderr, "short read, read %d frames\n", rc);
            alsa->npcm_input += rc;
         }
         else
            alsa->npcm_input += rc;

         if(alsa->input_npcm_once == alsa->npcm_input)
         {
		      auf.ts = stamp;
		      auf.samplestamp = stamp;
		      auf.raw = alsa->pcm_input;
		      auf.nraw = alsa->input_npcm_once;
		      auf.bytes = alsa->input_nbyte_once;
		      auf.eots = 1;
		      auf.sno = tick;

		      alsa_log(("\rpa_input_loop: in[%d] bytes[%d] frame#%lld ... ", pa->inbuf_r, auf.bytes, auf.sno));
		      alsa->receiver->receive_media(alsa->receiver, &auf, (int64)(auf.samplestamp), 0);
		      alsa_log(("done\n", pa->inbuf_r));

            memset(alsa->pcm_input, 0, alsa->input_nbyte_once);
            alsa->npcm_input = 0;
            stamp += alsa->input_npcm_once;
            tick++;
         }
      }
	}

	auf.ts = stamp;
	auf.samplestamp = stamp;
	auf.bytes = 0;
	auf.nraw = 0;
	auf.raw = alsa->pcm_input;
	auf.eots = 1;
	auf.sno = tick;

	alsa->receiver->receive_media(alsa->receiver, &auf, stamp, MP_EOS);

   alsa->npcm_input = 0;
   stamp += alsa->input_npcm_once;
   tick++;

   xfree(outbuf);
   
	return MP_OK;
}

media_pipe_t * alsa_pipe (media_device_t *dev)
{
   return ((alsa_device_t *)dev)->src_pipe;
}

int alsa_init (media_device_t * dev, media_control_t *ctrl)
{
   alsa_device_t *alsa = (alsa_device_t*)dev;
   
   alsa_card_manager_init(alsa->sndcard_manager, ALSA_CARD_NUMBER);
   
   return MP_OK;
}

control_setting_t* alsa_new_setting(media_device_t *dev)
{
   return NULL;
}
   
int alsa_setting (media_device_t * dev, control_setting_t *setting, module_catalog_t *cata)
{
   return MP_EIMPL;
}

int alsa_set_io (media_device_t *dev, media_info_t *minfo, media_receiver_t* recvr)
{
   alsa_device_t *alsa = (alsa_device_t*)dev;
   audio_info_t* ai = (audio_info_t*)minfo;

   if(!alsa->ai_input)
   {
      ai->info.sampling_constant = DEFAULT_OUTPUT_NSAMPLE_PULSE;
      ai->channels_bytes = ai->channels * ai->info.sample_bits / OS_BYTE_BITS;

      alsa->ai_input = ai;
      alsa->input_npcm_once = DEFAULT_INPUT_NSAMPLE_PULSE;
      alsa->input_nbyte_once = ai->channels_bytes * DEFAULT_INPUT_NSAMPLE_PULSE;

      alsa->pcm_input = xmalloc(alsa->input_nbyte_once);
      memset(alsa->pcm_input, 0, alsa->input_nbyte_once);

      alsa->receiver = recvr;
   }
   
   if(!alsa->ai_output)
   {
      alsa->ai_output = ai;
      alsa->output_npcm_once = DEFAULT_INPUT_NSAMPLE_PULSE;
      alsa->output_nbyte_once = ai->channels_bytes * DEFAULT_INPUT_NSAMPLE_PULSE;
      
      alsa->src_pipe = alsa_new_pipe (alsa);

      alsa_debug(("\ralsa_set_io: %d channels, %d rate, input bytes[%d]\n", ai->channels, ai->info.sample_rate, alsa->input_nbyte_once));
   }

   alsa->usec_pulse = 1000000 / ai->info.sample_rate * DEFAULT_OUTPUT_NSAMPLE_PULSE;

   return MP_OK;
}

int alsa_set_input (media_device_t *dev, media_info_t *minfo, media_receiver_t* recvr)
{
	return MP_EIMPL;
}
int alsa_set_output (media_device_t *dev, media_info_t *minfo)
{
	return MP_EIMPL;
}

int alsa_start(media_device_t *dev, int mode)
{
   audio_info_t *ai = NULL;
   
   alsa_device_t *alsa = (alsa_device_t*)dev;
   
   if(alsa->input_thread)
      return MP_OK;
      
   alsa->sndcard = alsa_card_new (alsa->devid);
   if(!alsa->sndcard)
      return MP_FAIL;
   
   if(alsa->ai_input && alsa->receiver)
   {
      ai = alsa->ai_input;
      alsa_card_open_r((AlsaCard*)alsa->sndcard, ai->info.sample_bits, ai->channels, ai->info.sample_rate);
   }

   if(alsa->ai_output)
   {
      ai = alsa->ai_output;
      alsa_card_open_w((AlsaCard*)alsa->sndcard, ai->info.sample_bits, ai->channels, ai->info.sample_rate);
   }

   alsa->input_thread = xthr_new (alsa_io_loop, alsa, XTHREAD_NONEFLAGS);

   return MP_OK;
}

int alsa_stop(media_device_t * dev, int mode)
{
   return MP_OK;
}

int alsa_done (media_device_t * dev)
{
   return MP_OK;
}

int alsa_match_mode(media_device_t *dev, char *mode)
{
   alsa_log(("alsa_match_type: input/output device\n"));

   if (!strcmp("output", mode)) return 1;
   if (!strcmp("input", mode)) return 1;
   if (!strcmp("inout", mode)) return 1;

   return 0;
}

int alsa_match_type(media_device_t *dev, char *type)
{
   if (strcmp("alsa", type) == 0)
      return 1;

   return 0;
}

module_interface_t* media_new_device ()
{
   media_device_t *dev = NULL;

   alsa_device_t * alsa = xmalloc (sizeof(struct alsa_device_s));
   if (!alsa)
   {

      alsa_debug(("media_new_device: No enough memory\n"));

      return NULL;
   }
   memset (alsa, 0, sizeof(struct alsa_device_s));

   dev = (media_device_t *)alsa;

   dev->init = alsa_init;
   dev->done = alsa_done;

   dev->start = alsa_start;
   dev->stop = alsa_stop;

   dev->pipe = alsa_pipe;

   dev->set_io = alsa_set_io;
   dev->set_input_media = alsa_set_input;
   dev->set_output_media = alsa_set_output;

   dev->new_setting = alsa_new_setting;
   dev->setting = alsa_setting;

   dev->match_type = alsa_match_type;
   dev->match_mode = alsa_match_mode;

   return dev;
}

/**
 * Loadin Infomation Block
 */
extern DECLSPEC module_loadin_t mediaformat =
{
   "device",   /* Label */

   000001,         /* Plugin version */
   000001,         /* Minimum version of lib API supportted by the module */
   000001,         /* Maximum version of lib API supportted by the module */

   media_new_device   /* Module initializer */
};
