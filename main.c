#define F_CPU 8000000UL 
#define USE_SERIAL

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <avr/eeprom.h>

#define WAIT_VALID_DATA     0
#define RECEIVE_AUTH        1
#define CHECK_AUTH_DATA     2
#define WAIT_OVERFLOW       3
#define WAIT_64BIT_START    4
#define RECEIVE_DATA        5
#define CHECK_RX_DATA       6

#define LED_ON              PORTC = 1
#define LED_OFF             PORTC = 0
#define THRESHOLD           1700

void USART_Init(unsigned int baud )
{
    /* Set baud rate */
    UBRRH = (unsigned char)(baud>>8);
    UBRRL = (unsigned char)baud;
    /* Enable receiver and transmitter */
    UCSRB = (1<<RXEN)|(1<<TXEN);
    /* Set frame format: 8data, 1stop bit */
    UCSRC = (1<<URSEL)|(3<<UCSZ0);
}

void USART_Transmit(unsigned char data )
{
    /* Wait for empty transmit buffer */
    while ( !( UCSRA & (1<<UDRE)) );
    /* Put data into buffer, sends the data */
    UDR = data;
}

void Serial_TX(unsigned int data)
{
    char str[16];
    unsigned char i;
    
    utoa(data, str, 10);
    for(i=0;i<16;i++)
    {
        if (str[i]!=0)
        USART_Transmit(str[i]);
        else
        break;
    }
    USART_Transmit(10);
    USART_Transmit(13);
    
}

//Print the 64bit received data
void Serial_Print_64(unsigned long data_l, unsigned long data_h)
{
    char hex[8];
    unsigned char i;
    
    sprintf(hex, "%lx", data_h);  
    for(i=0;i<(8-(strlen(hex)));i++)
        USART_Transmit(48);
    
    for(i=0;i<8;i++)
    {
        if (hex[i]!=0)
            USART_Transmit(hex[i]);
        else
            break;
    }

    sprintf(hex, "%lx", data_l);
    for(i=0;i<(8-(strlen(hex)));i++)
        USART_Transmit(48);
    
    for(i=0;i<8;i++)
    {
        if (hex[i]!=0)
            USART_Transmit(hex[i]);
        else
            break;
    }
    USART_Transmit(10);
    USART_Transmit(13);
    
}

//Setup input/output pins
void setup_pins()
{
    //TXD UART as output
    DDRD = (1 << PD1); 
    //Disable input pull-up resistor       
    PORTD = 0;
    //Set PB0 as input (ICP1)
    DDRB = 0;    
    //Enable pull-up resistor on PB0 
    PORTB = (1 << PB0);
    
    DDRC = (1 << PC0);
    PORTC = 0;
}

//Setup timer1
void setup_timer1()
{    
    //Setup TIMER1 to Fast PWM mode (OCR1A = TOP, overflow at TOP)   
    TCCR1A = (1 << WGM11) | (1 << WGM10);
    /*
    //Enable Input Capture Noise Canceler
    TCCR1B = (1 << ICNC1);
    //Input Capture Edge Select (1 = rising)
    TCCR1B |= (1 << ICES1);
    //(continue) Setting TIMER1 to fast PWM mode    
    TCCR1B |= (0 << WGM13) | (0 << WGM12);
    //No prescaler
    TCCR1B |= (1 << CS10);
    */
    TCCR1B = (1 << ICNC1) | (0 << ICES1) | (1 << CS10) | (1 << WGM13) | (1 << WGM12);
    //Clear Timer1 overflow interrupt
    TIFR = (1 << TOV1);
    //Set TOP at 5000 (5000 * 125ns = 625us)
    OCR1A = 5000;
    //No interrupt
    TIMSK = 0;
    //Reset Timer1
    TCNT1 = 0;
}

void reset_capture_unit()
{
    //Clear OV flag 
    TIFR = (1 << TOV1); 
    //Clear capture flag
    TIFR = (1 << ICF1);
    TCNT1 = 0;
    //Restart counter (previously stopped)
    TCCR1B = (1 << ICNC1) | (0 << ICES1) | (1 << CS10) | (1 << WGM13) | (1 << WGM12);
}

int main(void)
{    
    unsigned int    tmr = 0;    //Sampled value of TIMER1
    unsigned char   state = WAIT_VALID_DATA;
    unsigned char   next = WAIT_VALID_DATA;
    unsigned char   over = 0;   //Signal overflow condition
    unsigned long   rx_data=0;  //Received AUTH data
    unsigned long   data_l=0;   //Low 32bit received data sent by reader
    unsigned long   data_h=0;   //High 32bit received data sent by reader
    unsigned long   cnt = 0;    //Bit counter
    unsigned int    num_auth = 0; //Save the number of complete AUTH commands

    setup_pins();
    setup_timer1();
    USART_Init(51); //9600Baud
    
    LED_OFF;
    
    while(1)
    {
        switch (state)
        {
            case WAIT_VALID_DATA: 

                reset_capture_unit();
                while ((TIFR & (1 << ICF1)) == 0); //Wait for event on ICP1 pins
                
                //Stop counter
                TCCR1B = 0;
                //Sample timer value
                tmr = ICR1;
                //Reset counter
                cnt = 0;
                //Reset received data
                rx_data = 0;
                
                //Ignore if overflow
                if ((TIFR & (1 << TOV1)) != 0)
                    TIFR = (1 << TOV1); //Clear OV flag        
                else
                {
                    //Save data
                    if (tmr > THRESHOLD)
                        rx_data = (rx_data << 1) | 1;
                    else
                        rx_data = (rx_data << 1);
                    
                    cnt++;

                    //Data is valid, start saving data
                    next = RECEIVE_AUTH;
                }
            break; 
            
            case RECEIVE_AUTH: 
            
                reset_capture_unit();
                while ((TIFR & (1 << ICF1)) == 0) //Wait for event on ICP1 pins
                {
                    //Overflow condition (end of command)
                    if ((TIFR & (1 << TOV1)) != 0)
                    {
                        over = 1;
                        TIFR = (1 << TOV1); //Clear flag
                        break;
                    }
                }
                
                //Stop counter
                TCCR1B = 0;
                //Sample timer value
                tmr = ICR1;
                
                if (over)
                {
                    //Only AUTH data (5bits) are considered as valid
                    if (cnt != 5)
                        next = WAIT_VALID_DATA;
                    else
                        next = CHECK_AUTH_DATA; //Check current data
                }
                else
                {
                    //Save data
                    if (tmr > THRESHOLD)
                        rx_data = (rx_data << 1) | 1;
                    else
                        rx_data = (rx_data << 1);
                    
                    cnt++;
                }  
                
                over = 0;
                       
            break; 
            
            case CHECK_AUTH_DATA:

                //Check if i received an AUTH command
                if (rx_data == 0x18) //0x18 = 0x00011000
                {
                    next = WAIT_OVERFLOW;
                    _delay_ms(3);
                }
                else
                    next = WAIT_VALID_DATA; 
                
            break; 
            
            case WAIT_OVERFLOW:
            
                reset_capture_unit();
                
                while ((TIFR & (1 << ICF1)) == 0) //Wait for event on ICP1 pins
                {
                    if ((TIFR & (1 << TOV1)) != 0)
                    {
                        over = 1;
                        TIFR = (1 << TOV1); //Clear flag
                        break;
                    }
                }
                
                //Stop counter
                TCCR1B = 0;
                //Sample timer value
                tmr = ICR1;
                
                if (over)
                    next = WAIT_64BIT_START; 
                
                over = 0;             
                
            break; 
            
            case WAIT_64BIT_START:
    
                reset_capture_unit();
                while ((TIFR & (1 << ICF1)) == 0); //Wait for event on ICP1 pins
                
                //Stop counter
                TCCR1B = 0;
                //Sample timer value
                tmr = ICR1;
                
                //Clear rx data
                data_h = 0;
                data_l = 0;
                cnt = 0;
                
                //Ignore if overflow
                if ((TIFR & (1 << TOV1)) != 0)
                    TIFR = (1 << TOV1); //Clear OV flag        
                else
                {
                    //Save data
                    if (tmr > THRESHOLD)
                        data_h = (data_h << 1) | 1;
                    else
                        data_h = (data_h << 1);
                    
                    cnt++;
                    next = RECEIVE_DATA;
                }
                
            break; 
            
            case RECEIVE_DATA: 
            
                reset_capture_unit();
                while ((TIFR & (1 << ICF1)) == 0) //Wait for event on ICP1 pins
                {
                    //Overflow condition (end of command)
                    if ((TIFR & (1 << TOV1)) != 0)
                    {
                        over = 1;
                        TIFR = (1 << TOV1); //Clear flag
                        break;
                    }
                }
                
                //Stop counter
                TCCR1B = 0;
                //Sample timer value
                tmr = ICR1;
                
                if (over)
                {
                    if (cnt != 64) //Wrong data received
                        next = WAIT_64BIT_START;
                    else
                        next = CHECK_RX_DATA; //Check current data
                }
                else
                {
                    //Save data
                    if (cnt >= 32)
                    {
                        if (tmr > THRESHOLD)
                            data_l = (data_l << 1) | 1;
                        else
                            data_l = (data_l << 1);
                    }
                    else
                    {
                        if (tmr > THRESHOLD)
                            data_h = (data_h << 1) | 1;
                        else
                            data_h = (data_h << 1);
                    }

                    cnt++;
                }  
                
                over = 0;
                       
            break; 
            
            case CHECK_RX_DATA:
            
                LED_ON;
                //Save data inside EEPROM
                #ifdef USE_SERIAL
                    Serial_Print_64(data_l, data_h);
                #else
                if (num_auth == 0)
                {
                    eeprom_update_dword((uint32_t *)0,data_l);    
                    eeprom_update_dword((uint32_t *)4,data_h); 
                }
                else if (num_auth == 1)
                {
                    eeprom_update_dword((uint32_t *)8,data_l);    
                    eeprom_update_dword((uint32_t *)12,data_h);                   
                }
                #endif
                LED_OFF;
                
                next = WAIT_VALID_DATA;

                num_auth++;
                
            break; 
            
            default: 
                
            break;
        }
        
        state = next;
    }
}



