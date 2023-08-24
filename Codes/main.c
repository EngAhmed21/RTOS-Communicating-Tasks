#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "diag/trace.h"

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "semphr.h"

#define CCM_RAM __attribute__((section(".ccmram")))

// ----------------------------------------------------------------------------

#define queueLength				   3				// We set the length of the queue to be 3 at the first run then 10 in the next run
#define maxNumOfMessages		  1000
#define stackSize 				  1000
#define maxIndex 			   		5				// The maximum index in the lower and upper bounds arrays
#define receivedPeriodTicks		pdMS_TO_TICKS(100)	// The period in Ticks of the receiver

QueueHandle_t messageQueue;

SemaphoreHandle_t senderSemaphore[3];
SemaphoreHandle_t receiverSemaphore;

TimerHandle_t senderTimer[3];
TimerHandle_t receiverTimer;


static int32_t blockedMessages[3] = {0};
static int32_t sentMessages[3] = {0};
static int32_t receivedMessages = 0;

static int8_t currentIndex = -1;

static int32_t sumOfSenderTime[3] = {0};		// It represents the sum of T sender1 at each iteration and we use it to get the average time

static int32_t numOfIterationsForSender[3] = {0};		// The number of times at which the period of the timer has changed


const int lowerBounds[] = {50, 80, 110, 140, 170, 200};
const int upperBounds[] = {150, 200, 250, 300, 350, 400};

const char str[] = "Time is ";


// Sample pragmas to cope with warnings. Please note the related line at
// the end of this function, used to pop the compiler diagnostics status.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#pragma GCC diagnostic ignored "-Wreturn-type"



int randTimeGenerator (int lowerBound, int upperBound);		// Returns a random number bounded by a maximum and a minimum numbers

void initializer();		// Initialize the system

void SenderTask1( void *parameters );
void SenderTask2( void *parameters );
void SenderTask3( void *parameters );
static void ReceiverTask( void *parameters );

void Sender1_TimerCallback(TimerHandle_t xTimer);
void Sender2_TimerCallback(TimerHandle_t xTimer);
void Sender3_TimerCallback(TimerHandle_t xTimer);
void ReceiverTimerCallback(TimerHandle_t xTimer);

void Reset();


int main( void )
{
	messageQueue = xQueueCreate( queueLength, sizeof( char[30] ) );

	if( messageQueue != NULL )
	{ // check if creation succeed, ignored to simplify

		// call Reset function
		Reset();

		// Initialize the random number generator
		srand(time(NULL));

		// call the initializer
	  	initializer();

		// Start the scheduler
	   	vTaskStartScheduler();
	}
	else
	{
		// queue could not be created.
		printf("The queue can't be created");
		while(1);
	}
}


void initializer()
{
	int senderTime[3] = {0};
	for(int i = 0; i < 3; i++)
	{
		// Create sender and receiver timers
		senderTime[i] = randTimeGenerator(lowerBounds[currentIndex], upperBounds[currentIndex]);
		if(i == 0)
		{
			senderTimer[i] = xTimerCreate("Sender1_Timer", pdMS_TO_TICKS(senderTime[i]), pdTRUE, NULL, Sender1_TimerCallback);
		}
		else if(i == 1)
		{
			senderTimer[i] = xTimerCreate("Sender2_Timer", pdMS_TO_TICKS(senderTime[i]), pdTRUE, NULL, Sender2_TimerCallback);
		}
		else
		{
			senderTimer[i] = xTimerCreate("Sender3_Timer", pdMS_TO_TICKS(senderTime[i]), pdTRUE, NULL, Sender3_TimerCallback);
		}

		// Update the value of the sum of Tsender
		sumOfSenderTime[i] += senderTime[i];

	    // Update the value of the number of iterations
	    numOfIterationsForSender[i]++;

	    // Create sender semaphores
	    senderSemaphore[i] = xSemaphoreCreateBinary();
	}

	 // Create the receiver timer
	receiverTimer = xTimerCreate("ReceiverTimer", receivedPeriodTicks, pdTRUE, NULL, ReceiverTimerCallback);

	 // Create the receiver semaphore
	receiverSemaphore = xSemaphoreCreateBinary();

	// Create sender and receiver tasks
	xTaskCreate( SenderTask1, "Sender1", stackSize , NULL, 1, NULL );
	xTaskCreate( SenderTask2, "Sender2", stackSize , NULL, 1, NULL );
	xTaskCreate( SenderTask3, "Sender3", stackSize , NULL, 2, NULL );
	xTaskCreate( ReceiverTask, "Receiver", stackSize, NULL, 3, NULL );

	// Start the timers
   	for(int i = 0; i < 3; i++)
   	{
   	   	xTimerStart(senderTimer[i], 0);
   	}
   	xTimerStart(receiverTimer, 0);
}

int randTimeGenerator (int lowerBound, int upperBound)
{
	 int randomVariable = lowerBound + rand() % (upperBound - lowerBound + 1);
	 return randomVariable;
}

void SenderTask1( void *parameters )
{
	int senderTime1;
	int32_t ticks;
	char lValueToSend[30];
	BaseType_t Status;
	/* As per most tasks, this task is implemented within an infinite loop. */
	while(1)
	{
		// Take the semaphore
	    xSemaphoreTake(senderSemaphore[0], portMAX_DELAY);

	    // Change the period of the timer
		senderTime1 = randTimeGenerator(lowerBounds[currentIndex], upperBounds[currentIndex]);
	    xTimerChangePeriod(senderTimer[0], pdMS_TO_TICKS(senderTime1), 0);

	    // Updating the sum of Tsender and the number of iterations
	    sumOfSenderTime[0] += senderTime1;
	    numOfIterationsForSender[0]++;

	    // Get the value to be sent
	    ticks = xTaskGetTickCount();
	    sprintf(lValueToSend, "%s%ld", str, ticks);
	    // Send the value to the queue
		Status = xQueueSend( messageQueue, &lValueToSend, 0 );
		if ( Status != pdPASS ) { //failed
			blockedMessages[0]++;
			//printf( "Could not send to the queue.\r\n" );
		}
		else
			sentMessages[0]++;
	}
}

void SenderTask2( void *parameters )
{
	int senderTime2;
	int32_t ticks;
	char lValueToSend[30];
	BaseType_t Status;
	/* As per most tasks, this task is implemented within an infinite loop. */
	while(1)
	{
	    xSemaphoreTake(senderSemaphore[1], portMAX_DELAY);

	    senderTime2 = randTimeGenerator(lowerBounds[currentIndex], upperBounds[currentIndex]);
	    xTimerChangePeriod(senderTimer[1], pdMS_TO_TICKS(senderTime2), 0);

	    sumOfSenderTime[1] += senderTime2;
	    numOfIterationsForSender[1]++;

	    ticks = xTaskGetTickCount();
	    sprintf(lValueToSend, "%s%ld", str, ticks);

	    Status = xQueueSend( messageQueue, &lValueToSend, 0 );
		if ( Status != pdPASS )
		{ //failed
			blockedMessages[1]++;
			//printf( "Could not send to the queue.\r\n" );
		}
		else
			sentMessages[1]++;
	}
}


void SenderTask3( void *parameters )
{
	int senderTime3;
	int32_t ticks;
	char lValueToSend[30];
	BaseType_t Status;
	/* As per most tasks, this task is implemented within an infinite loop. */
	while(1)
	{
	    xSemaphoreTake(senderSemaphore[2], portMAX_DELAY);

	    senderTime3 = randTimeGenerator(lowerBounds[currentIndex], upperBounds[currentIndex]);
	    xTimerChangePeriod(senderTimer[2], pdMS_TO_TICKS(senderTime3), 0);

	    sumOfSenderTime[2] += senderTime3;
	    numOfIterationsForSender[2]++;

	    ticks = xTaskGetTickCount();
	    sprintf(lValueToSend, "%s%ld", str, ticks);

	    Status = xQueueSend( messageQueue, &lValueToSend, 0 );
		if ( Status != pdPASS ) { //failed
			blockedMessages[2]++;
			//printf( "Could not send to the queue.\r\n" );
		}
		else
			sentMessages[2]++;
	}
}


static void ReceiverTask( void *parameters )
{
	char receivedValue[30];
	BaseType_t status;
	while(1)
	{
		xSemaphoreTake(receiverSemaphore, portMAX_DELAY);

		status = xQueueReceive( messageQueue, &receivedValue, 0 );
		if( status == pdPASS )
		{
			//printf( "Received = %s\r\n", receivedValue ); //success
			receivedMessages++;
		}
		else
		{
			//printf( "Could not receive from the queue.\r\n" );
		}
		if(receivedMessages == maxNumOfMessages)
			Reset();
	}
}

void Sender1_TimerCallback(TimerHandle_t xTimer) {
    // Release senderSemaphore to unblock sender task
    xSemaphoreGive(senderSemaphore[0]);
}

void Sender2_TimerCallback(TimerHandle_t xTimer) {
    // Release senderSemaphore to unblock sender task
    xSemaphoreGive(senderSemaphore[1]);
}

void Sender3_TimerCallback(TimerHandle_t xTimer) {
    // Release senderSemaphore to unblock sender task
    xSemaphoreGive(senderSemaphore[2]);
}

void ReceiverTimerCallback(TimerHandle_t xTimer) {
    // Release receiverSemaphore to unblock receiver task
    xSemaphoreGive(receiverSemaphore);
}

void Reset()
{
	printf("currentIndex = %d\n", currentIndex);
	if(currentIndex >= 0)
	{
		int32_t avgTimeSender[3];
		for(int i = 0; i < 3; i++)
		{
			avgTimeSender[i] = sumOfSenderTime[i] / numOfIterationsForSender[i];
			printf("Average Tsender%d = %ld\n", (i + 1), avgTimeSender[i]);
		}
		int32_t avgTotalSenderTime = (avgTimeSender[0] + avgTimeSender[1] + avgTimeSender[2]) / 3;
		printf("Average Tsender = %ld\n", avgTotalSenderTime);
	}

	// Print the total sent and blocked messages
	int32_t totalSuccessMessages = sentMessages[0] + sentMessages[1] + sentMessages[2];
	int32_t totalBlockedMessages = blockedMessages[0] + blockedMessages[1] + blockedMessages[2];
	printf("Total successfully sent messages: %ld\n", totalSuccessMessages);
	printf("Total blocked messages: %ld\n", totalBlockedMessages);

	for(int i = 0; i < 3; i++)
		{
			// Print counters
			printf("For snderTask%d:\t sent = %ld\t blocked = %ld\n", (i + 1), sentMessages[i], blockedMessages[i]);

			// Reset counters
			sentMessages[i] = 0;
			blockedMessages[i] = 0;
			sumOfSenderTime[i] = 0;
		    numOfIterationsForSender[i] = 0;
		}

	//
	printf("\n");
	// Reset receivedMessages counter
	receivedMessages = 0;

	// Clear the queue
    xQueueReset(messageQueue);

    // Configure next timer period
    currentIndex++;
    if (currentIndex > maxIndex)
    {
	   // All timer periods used, stop execution
    	printf("Game Over\n");
    	while (1);
    }
}



void vApplicationMallocFailedHook( void )
{
	/* Called if a call to pvPortMalloc() fails because there is insufficient
	free memory available in the FreeRTOS heap.  pvPortMalloc() is called
	internally by FreeRTOS API functions that create tasks, queues, software
	timers, and semaphores.  The size of the FreeRTOS heap is set by the
	configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	/* Run time stack overflow checking is performed if
	configconfigCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected. */
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook( void )
{
volatile size_t xFreeStackSpace;

	/* This function is called on each cycle of the idle task.  In this case it
	does nothing useful, other than report the amout of FreeRTOS heap that
	remains unallocated. */
	xFreeStackSpace = xPortGetFreeHeapSize();

	if( xFreeStackSpace > 100 )
	{
		/* By now, the kernel has allocated everything it is going to, so
		if there is a lot of heap remaining unallocated then
		the value of configTOTAL_HEAP_SIZE in FreeRTOSConfig.h can be
		reduced accordingly. */
	}
}

void vApplicationTickHook(void) {
}

StaticTask_t xIdleTaskTCB CCM_RAM;
StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE] CCM_RAM;

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize) {
  /* Pass out a pointer to the StaticTask_t structure in which the Idle task's
  state will be stored. */
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

  /* Pass out the array that will be used as the Idle task's stack. */
  *ppxIdleTaskStackBuffer = uxIdleTaskStack;

  /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
  Note that, as the array is necessarily of type StackType_t,
  configMINIMAL_STACK_SIZE is specified in words, not bytes. */
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

static StaticTask_t xTimerTaskTCB CCM_RAM;
static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH] CCM_RAM;

/* configUSE_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
application must provide an implementation of vApplicationGetTimerTaskMemory()
to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize) {
  *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
  *ppxTimerTaskStackBuffer = uxTimerTaskStack;
  *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
