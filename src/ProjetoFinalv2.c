if(isStopped == true && isWaitingCmd == false)
        {
          UARTprintf("ef\r");
          if(string[0] == 'e' && string[1] == 'E')
          {
            level = (int) string[3];
          }
          if(string[0] == 'e' && string[1] == 'I')
          {
            level_internal = (int) string[3];
          }
          if(level > currentFloor)
          {
            UARTprintf("es\r");
            isStopped = false;
            isAscending = true;
            isGoingDown = false;
          }
          if(level < currentFloor)
          {
            UARTprintf("ed\r");
            isStopped = false;
            isAscending = false;
            isGoingDown = true;
          }
        }
        
        if(isStopped == false)
        {
          if(string[0] == 'e')
          {
            currentFloor = (int) string[1]; //Atualiza o andar do elevador
          }
          if(isAscending == true && isGoingDown == false && isWaitingCmd == false)
          {
            if((int) string[1] == level)
            {
              UARTprintf("ep\r");
              //TODO - forma de deixar elevador proximo da altura ideal
              //osDelay(50);
              //UARTprintf("ep\r");
              // END TODO
              isWaitingCmd = true;
              isStopped = true;
            }
          }
          
          if(isAscending == false && isGoingDown == true && isWaitingCmd == false)
          {
            if((int) string[1] == level)
            {
              UARTprintf("ep\r");
              isWaitingCmd = true;
              isStopped = true;
            }
          }
          
          if(isWaitingCmd == true)
          {
            if(string[0] == 'e' && string[1] == 'I')
            {
              char button = (string[2]);
              char str[10];
              strcpy(str, "");
              str[0] = 'e';
              str[1] = 'L';
              str[2] = button;
              str[3] = '\r';
              floorsInternal_leftElevator[(int)button - 97] = 1;
              UARTprintf("%s", str);
            }
            if(string[0] == 'e' && string[1] == 'E')
            {
              char button = ((int) string[2]) - 97;
              //UARTprintf("\r");
            }
          }
        }