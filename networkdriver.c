/*
Emmalie Dion
emmalie
CIS 415 Project 2
This is my own work. I discussed implementation with Matt Almenshad, but we have different solutions.
I also utilize functions provided by Professor Sventek in the BoundedBuffer.h, freepacketdescriptorstore.h,
freepacketdescriptorstore__full.h, networkdevice.h, packetdescriptorcreator.h, packetdescriptor.h, and pid.h (MAX_PID)
*/


#include "BoundedBuffer.h"
#include "diagnostics.h"
#include "freepacketdescriptorstore.h"
#include "freepacketdescriptorstore__full.h"
#include "networkdevice.h"
#include "networkdriver.h"
#include "packetdescriptorcreator.h"
#include "packetdescriptor.h"
#include "pid.h"
#include "pthread.h"

/* Global variables */
BoundedBuffer *rec_buffer[MAX_PID + 1]; /*buffer for received packets*/
BoundedBuffer *send_buffer; /*buffer for sent packet descriptors*/
NetworkDevice *n; 
FreePacketDescriptorStore *store;
pthread_t rec; /*receiving thread*/
pthread_t send; /*sending thread*/

#define UNUSED __attribute__ ((unused))

/* send_thread: This function handles the thread that sends packet descriptors to the network device */
void * send_thread(UNUSED void *st){

    int i;
    int flag = 1;  /*if the packet descriptor failed to send*/

    PacketDescriptor* desc = blockingReadBB(send_buffer);  /*performs a blocking read to send packetdescriptors to the network device*/

    while(1){

        for(i=1; i < 5 && flag; i++){  /*if the packet descriptor succeeds in sending and the number of failed attempts is less than 4*/

        	if((send_packet(n,desc))){  /*send the packet descriptor to the network device*/
          
                nonblocking_put_pd(store, desc);   /*put the empty packet descriptor back in the store*/
                desc = blockingReadBB(send_buffer); /*the next element in the buffer is next in line to be sent*/
                flag = 0; /*set flag to false*/
            }   
        }

        if(flag){  /*if the packet descriptor failed to send four times*/

            nonblocking_put_pd(store, desc); /*put the packet descriptor back in the store*/
            desc = blockingReadBB(send_buffer); /*the next element in the buffer is next in line to be sent*/

        }

        flag = 1;  /*set flag to true*/
            
    }
}

/* receive_thread: This function handles the thread that receives packets from the network device and then sends them to the 
appropriate applications */
void * receive_thread(UNUSED void *rt){


    PacketDescriptor* new_d = NULL;
    PacketDescriptor* temp = NULL;
    PID index;
    blocking_get_pd(store, &new_d); /* get a packet descriptor from the store */
    

    while(1){

    	if(nonblocking_get_pd(store, &temp)){ /*if the next packet descriptor is successfully grabbed from the store*/

            init_packet_descriptor(new_d); /*initialize the packet descriptor*/
            register_receiving_packetdescriptor(n, new_d); /*register the packet descriptor with the network device*/
            await_incoming_packet(n); /*wait for a packet to arrive from the network device*/
            index = packet_descriptor_get_pid(new_d); /*packet descriptor index is the same as pid (between 0 and 10) - tells which buffer to write to*/
            if(!nonblockingWriteBB(rec_buffer[index], new_d)){  /*if the packet cannot be written into the appropriate application buffer (buffer is full)*/

                nonblocking_put_pd(store, new_d); /*return the packet to the store*/

            }
            new_d = temp; /* store the new packet descriptor into the temporary packet descriptor*/
    	}
    	else{  /* If a packet descriptor could not be grabbed from the store*/

    		init_packet_descriptor(new_d); /* initialize previous packet descriptor*/
            register_receiving_packetdescriptor(n, new_d); /* register the packet descriptor with the network device*/
            await_incoming_packet(n); /* wait for a packet to arrive from the network device */

    	}
	
    }
    
}

/* init_network_driver: initializes the free packet descriptor store and populates it with packet descriptors,
creates buffers for the sending and receiving thread to use, and creates the sending and receiving pthreads*/
void init_network_driver(NetworkDevice *nd, void *mem_start,
 unsigned long mem_length, FreePacketDescriptorStore **fpds) {

 	int i;
 	n = nd;

	store = create_fpds(); /*create free packet descriptor store*/

	create_free_packet_descriptors(store, mem_start, mem_length); /*fill the store with packet descriptors*/
 
	send_buffer = createBB(12); /*create a bounded buffer for the sending thread*/
    for(i = 0; i < (MAX_PID+1); i++){ 

    	rec_buffer[i] = createBB(2); /*create a receiving buffer for each application*/
    }
 
    pthread_create(&rec, NULL, &receive_thread, NULL); /*pthread to handle receiving packets from the network device and sending them to appropriate applications*/
    pthread_create(&send, NULL, &send_thread, NULL); /*pthread to handle sending packet descriptors to the network device*/

    *fpds = store;

}

/* blocking_send_packet: queues up packet descriptors to be sent to the network device*/
void blocking_send_packet(PacketDescriptor *pd) {

	blockingWriteBB(send_buffer, pd); /*don't return until success*/
}

/* nonblocking_send_packet: queue up packet descriptors, but instead of waiting for success send a message of success or failure*/
int nonblocking_send_packet(PacketDescriptor *pd) {
 
	return nonblockingWriteBB(send_buffer, pd); /*return 1 if the packet descriptor can be queued immediately or 0 if it cannot*/
}

/* blocking_get_packet: get a packet from the network driver and return the packet descripter to the application*/
void blocking_get_packet(PacketDescriptor **pd, PID pid) {
 
    *pd = (PacketDescriptor*) blockingReadBB(rec_buffer[pid]); /*wait until there is a packet and then read the packet descriptor into the corresponding application buffer*/

}

/* nonblocking_get_packet: get a packet from the network driver if one is available and give it to calling application, or return 0 if there is no packet*/
int nonblocking_get_packet(PacketDescriptor **pd, PID pid) {
 
	return nonblockingReadBB(rec_buffer[pid], (void *) pd); /*if there is a packet in network driver then grab it and send it to calling application (no waiting)*/
}

