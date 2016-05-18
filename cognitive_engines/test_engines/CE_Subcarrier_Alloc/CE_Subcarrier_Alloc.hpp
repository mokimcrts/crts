#ifndef _CE_SUBCARRIER_ALLOC_
#define _CE_SUBCARRIER_ALLOC_

#include "CE.hpp"
#include <sys/time.h>

class CE_Subcarrier_Alloc : public Cognitive_Engine {

private:
  struct timeval tv;
  time_t switch_time_s;
  int period_s;
  int first_execution;

  char custom_alloc[32];
  int alloc;

public:
  CE_Subcarrier_Alloc(int argc, char **argv, ExtensibleCognitiveRadio *_ECR);
  ~CE_Subcarrier_Alloc();
  virtual void execute();
};

#endif