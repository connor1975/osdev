#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

int is_delim(char c, char *delim){
  while(*delim != '\0')
  {
    if(c == *delim)
      return 1;
    delim++;
  }
  return 0;
}

char *strtok_r(char *s, char *delim,char** saveptr){
  	if(s == NULL){
		if(saveptr == NULL || *saveptr == NULL) return NULL;
	  	s = *saveptr;
  	}

  	while(1){
  	  	if(is_delim(*s, delim) && *s != 0){
  	  	  s++;
  	  	  continue;
  	  	}
		if(*s == 0) return NULL;
  	  	break;
  	}

  	char *ret = s;
  	while(1)
  	{
  	  	if(*s == 0){
  	  		*saveptr = s;
  	  		return ret;
  	  	}
  	  	if(is_delim(*s, delim)){
  	  	  	*s = 0;
  	  	  	*saveptr = s + 1;
  	  	  	return ret;
  	  	}
  	  	s++;
  	}
}