#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include "bank_strucs.h"

#define PORT 15001 
#define SA struct sockaddr
#define MAX_BUFF_SIZE 100
#define CARD_VALID_MSG "CARD_VALID"
#define SUM_SUCCESSFULLY_DEPOSITED_MSG "SUCCESS_DEPOSIT"
#define SUM_NOT_INTEGER_MSG "SUM_NOT_INTEGER"
#define SUM_INVALID_MSG "SUM_INVALID"
#define ACCOUNT_BALANCE_NOT_ENOUGH_MSG "ACCOUNT_BALANCE_NOT_ENOUGH"
#define PROBLEM_RECEIVING_PIN_CODE_MSG "PROBLEM_RECEIVING_PIN_CODE"
#define PIN_CODE_RECEIVED_MSG "PIN_CODE_RECEIVED"
#define CASH_RECEIPT_YES_MSG "CASH_RECEIPT_YES"
#define CASH_RECEIPT_NO_MSG "CASH_RECEIPT_NO"
#define CASH_RECEIPT_INVALID_MSG "CASH_RECEIPT_NO"
#define CARD_NUMBER_LEN 16
#define PIN_CODE_LEN 4
#define MIN_WITHDRAW_LIMIT 10
#define MAX_WITHDRAW_LIMIT 1000
#define MAX_PIN_CODE_ENTRIES 3
#define LANG_EN 1
#define LANG_BG 2
#define CASH_RECEIPT_YES 1
#define CASH_RECEIPT_NO 2
#define SUM_10_OPTION 1
#define SUM_20_OPTION 2
#define SUM_30_OPTION 3
#define SUM_50_OPTION 4
#define SUM_100_OPTION 5
#define SUM_CUSTOM_OPTION 6
#define SUM_10 10
#define SUM_20 20
#define SUM_30 30
#define SUM_50 50
#define SUM_100 100

// 4169910013514509 4167910013514510 4179910013514610 
//#define CARD_NUMBER "4169910013514509"
//#define CARD_NUMBER "4167910013514510"
#define CARD_NUMBER "4179910013514610"

int atm_start(int sockfd);
int validate_card(int socket);
int receive_pin_code(int socket, char *pin_code);
int start_ui(int socket, char *pin_code);
int select_language();
int get_sum(int *withdraw);
int read_withdraw_option(int *option);
int read_custom_withdraw(int *withdraw);
int read_pin_code(char *pin_code_input);
int read_valdiate_pin_code(char *pin_code);
int send_sum_to_bank(int socket, int sum_withdraw);
int confirm_deposit(int socket);
int cash_receipt(int socket);
int read_cash_receipt_option(int *cash_receipt_option);
int read_cash_receipt(int socket, char *buff);
int read_int(char *buff, char **end, int *value);
void print_languages_menu();
void print_sums_menu();
void print_sums_menu_en();
void print_sums_menu_bg();
void print_custom_withdraw_menu();
void print_custom_withdraw_menu_en();
void print_custom_withdraw_menu_bg();
void print_pin_code_menu();
void print_pin_code_menu_en();
void print_pin_code_menu_bg();
void print_invalid_pin_code_msg(int attempt_count);
void print_invalid_pin_code_msg_en(int attempt_count);
void print_invalid_pin_code_msg_bg(int attempt_count);
void print_balance_error_msg();
void print_balance_error_msg_en();
void print_balance_error_msg_bg();
void print_cash_receipt_menu();
void print_cash_receipt_menu_en();
void print_cash_receipt_menu_bg();
void print_cash_receipt(char *buff);
void print_cash_receipt_en(char *timestamp, char *withdraw_sum, char *balance);
void print_cash_receipt_bg(char *timestamp, char *withdraw_sum, char *balance);
void print_error_msg();
void print_error_msg_en();
void print_error_msg_bg();
void send_response_status_msg(int socket, char *msg);

int current_language = 0;

int main()
{
	int server_socket;
 	struct sockaddr_in server_addr;

 	server_socket = socket(AF_INET, SOCK_STREAM, 0);
 	if (server_socket == -1) {
  		perror("Socket creation failed. Reason ");
  		exit(0);
 	}
 	else
	{
  		//printf("Socket successfully created..\n");
	}

 	bzero(&server_addr, sizeof(server_addr));
 
 	server_addr.sin_family = AF_INET;
 	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
 	server_addr.sin_port = htons(PORT);

 	if (connect(server_socket, (SA*)&server_addr, sizeof(server_addr)) != 0)
	{
  		perror("Connection with the server failed. Reason ");
  		exit(0);
 	}
 	else
	{
  		//printf("Connected to the server..\n");
	}

 	int status_code = atm_start(server_socket);

	if (status_code != 0)
	{
        if (status_code < 0)
        {
		    print_error_msg();
        }
		//printf("There was an error while communicating with the bank. ATM status code: %d\n", status_code);
	}

 	close(server_socket);

	return 0;
}

int atm_start (int socket)
{
	//printf("Connected to bank server...\n");

	int status_code = validate_card(socket);
	if (status_code != 0)
	{
		//printf("Error validating card number\n");
		return status_code;
	}

	//printf("Card is valid, receiving pin code from bank...\n");

	char pin_code[PIN_CODE_LEN + 1];
	receive_pin_code(socket, pin_code);

	//printf("Received pin code: %s\n", pin_code);

	status_code = start_ui(socket, pin_code);
    if (status_code != 0)
    {
        //printf("Error while processing user request.\n");
        return status_code;
    }

	//printf("Disconnecting...\n");
	return 0;
}

int start_ui(int socket, char *pin_code)
{
	//printf("Starting ATM UI...\n");

    int status_code = select_language();
    if (status_code != 0)
    {
        //printf("Error while reading language option. \n");
        return status_code;
    }

    int sum_withdraw;
    status_code = get_sum(&sum_withdraw);
    if (status_code != 0)
    {
        //printf("Error while reading sum for withdraw\n");
        return status_code;
    }

	//printf("Sum for withdraw: %d\n", sum_withdraw);

	status_code = read_valdiate_pin_code(pin_code);
	if (status_code != 0)
	{
		//printf("Error while validating pin code.\n");
		return status_code;
	}

	//printf("PIN CODE IS VALID\n");

	status_code = send_sum_to_bank(socket, sum_withdraw);
	if (status_code != 0)
	{
		//printf("Error while sending sum to bank.\n");
		return status_code;
	}

	//printf("Sum successfully sent to bank.\n");

	status_code = confirm_deposit(socket);
	if (status_code != 0)
	{
		//printf("Error while depositing sum in bank.\n");
		return status_code;
	}

	//printf("Sum successfully deposited in account.\n");

    status_code = cash_receipt(socket);
    if (status_code != 0)
    {
        //printf("Error while processing cash receipt.\n");
        return status_code;
    }

	//printf("Exiting ATM UI...\n");
    return 0;
}

int validate_card(int socket)
{
	char buff[MAX_BUFF_SIZE];
	char card_number[CARD_NUMBER_LEN + 1] = CARD_NUMBER;

	if (write(socket, card_number, sizeof(card_number)) != sizeof(card_number))
	{
		perror("Error while sending card number: ");
		return -1001;
	}

	if (read(socket, buff, sizeof(buff)) < 0)
	{
		perror("Error while receiving card valid msg. Reason ");
		return -1002;
	}

	//printf("validate_card buff: %s\n", buff);

	if (strcmp(buff, CARD_VALID_MSG) != 0)
	{
		//printf("Error while validating card number. Bank status code: %s\n", buff);
		return -2001;
	}

	return 0;
}

int receive_pin_code(int socket, char *pin_code)
{
	char buff[MAX_BUFF_SIZE];
	memset(buff, 0, sizeof(buff));

	while (strlen(buff) != PIN_CODE_LEN)
	{
		if (read(socket, buff, sizeof(buff)) < 0)
		{
			perror("Error while reading pin code. Reason ");
			return -1003;
		}

		if (strlen(buff) == PIN_CODE_LEN)
		{
			send_response_status_msg(socket, PIN_CODE_RECEIVED_MSG);
			break;
		}

		//printf("Retrying to get pin code...\n");
		send_response_status_msg(socket, PROBLEM_RECEIVING_PIN_CODE_MSG);
	}

	strncpy(pin_code, buff, PIN_CODE_LEN + 1);
	//printf("pin code received: %s\n", pin_code);

	return 0;
}

int select_language()
{
    char *end;
    char buff[MAX_BUFF_SIZE];

    do {
        clrscr();
        print_languages_menu();

        int status_code = read_int(buff, &end, &current_language);
        if (status_code != 0)
        {
            //printf("Error while reading language option.\n");
            return status_code;
        }
    } while (end != buff + strlen(buff)
                || buff[0] == 0 || current_language < 1 || current_language > 2);

    return 0;
}

int get_sum(int *withdraw)
{
    int withdraw_option = 0;
    int status_code = read_withdraw_option(&withdraw_option);
    if (status_code != 0)
    {
        //printf("Error while reading withdraw option.");
        return status_code;
    }

    switch (withdraw_option)
    {
        case SUM_10_OPTION:
            *withdraw = SUM_10;
            break;
        case SUM_20_OPTION:
            *withdraw = SUM_20;
            break;
        case SUM_30_OPTION:
            *withdraw = SUM_30;
            break;
        case SUM_50_OPTION:
            *withdraw = SUM_50;
            break;
        case SUM_100_OPTION:
            *withdraw = SUM_100;
            break;
        case SUM_CUSTOM_OPTION:
            status_code = read_custom_withdraw(withdraw);
            if (status_code != 0)
            {
                //printf("Error while reading custom withdraw.");
                return status_code;
            }
            break;
        default:
            //printf("Invalid withdraw option");
            return -2002;
    }

    return 0;
}

int read_withdraw_option(int *option)
{
    char *end;
    char buff[MAX_BUFF_SIZE];

    do {
        clrscr();
        print_sums_menu();

        int status_code = read_int(buff, &end, option);
        if (status_code != 0)
        {
            //printf("Error while reading integer.\n");
            return status_code;
        }
    } while (end != buff + strlen(buff)
                || buff[0] == 0 || *option < 1 || *option > 6);

    return 0;
}

int read_custom_withdraw(int *withdraw)
{
    char *end;
    char buff[MAX_BUFF_SIZE];

    do {
        clrscr();
        print_custom_withdraw_menu();

        int status_code = read_int(buff, &end, withdraw);
        if (status_code != 0)
        {
            //printf("Error while reading integer.\n");
            return status_code;
        }
    } while (end != buff + strlen(buff)
                || buff[0] == 0 || *withdraw < MIN_WITHDRAW_LIMIT
				|| *withdraw > MAX_WITHDRAW_LIMIT || *withdraw % 10 != 0);

    return 0;
}

int read_valdiate_pin_code(char *pin_code)
{
	char pin_code_buff[MAX_BUFF_SIZE];
	int i;

	clrscr();
	for(i = 0; i < MAX_PIN_CODE_ENTRIES; i++)
	{
		print_pin_code_menu();

		int status_code = read_pin_code(pin_code_buff);
		if (status_code != 0)
		{
			//printf("Error while reading pin code.\n");
			return status_code;
		}

		if (strcmp(pin_code, pin_code_buff) == 0)
		{
			break;
		}

		clrscr();
		print_invalid_pin_code_msg(i + 1);
	}

	if (i >= MAX_PIN_CODE_ENTRIES)
	{
		//printf("Max pin code attempts reached.");
		return 1002;
	}

	return 0;
}

int read_pin_code(char *pin_code_buff)
{
	if (!fgets(pin_code_buff, MAX_BUFF_SIZE, stdin))
	{
		perror("Error while reading from stdin. Reason ");
		return -1004;
	}

	pin_code_buff[strlen(pin_code_buff) - 1] = 0;

    return 0;
}

int send_sum_to_bank(int socket, int sum_withdraw)
{
	char sum_withdraw_string[MAX_BUFF_SIZE];
	if (sprintf(sum_withdraw_string, "%d", sum_withdraw) < 0)
	{
		perror("Error while converting sum to string. Reason ");
		return -1006;
	}

	if (write(socket, sum_withdraw_string, sizeof(sum_withdraw_string)) != sizeof(sum_withdraw_string))
	{
		perror("Error while sending sum. Reason ");
		return -1007;
	}

	return 0;
}

int confirm_deposit(int socket)
{
	char buff[MAX_BUFF_SIZE];

	if (read(socket, buff, sizeof(buff)) < 0)
	{
		perror("Error while getting confirmation from bank. Reason ");
		return -1008;
	}

	if (strcmp(buff, ACCOUNT_BALANCE_NOT_ENOUGH_MSG) == 0)
	{
		print_balance_error_msg();
		return 1001;
	}
	else if (strcmp(buff, SUM_SUCCESSFULLY_DEPOSITED_MSG) != 0)
    {
        //printf("Error while validating bank confirmation. Bank status code: %s\n", buff);
        return -2003;
    }

	return 0;
}

int cash_receipt(int socket)
{
	int cash_receipt_option;
	char buff[MAX_BUFF_SIZE];

	read_cash_receipt_option(&cash_receipt_option);
	if (cash_receipt_option == CASH_RECEIPT_YES)
	{
		send_response_status_msg(socket, CASH_RECEIPT_YES_MSG);
		int status_code = read_cash_receipt(socket, buff);
		if (status_code != 0)
		{
			//printf("Error while reading cash recept.\n");
			return status_code;
		}

		clrscr();
		print_cash_receipt(buff);
	}
	else if (cash_receipt_option == CASH_RECEIPT_NO)
	{
		send_response_status_msg(socket, CASH_RECEIPT_NO_MSG);
	}
	else
	{
		//printf("There was an error while processing your cash receipt request...\n");
		return -1010;
	}

	return 0;
}

int read_cash_receipt_option(int *cash_receipt_option)
{
	char *end;
	char buff[MAX_BUFF_SIZE];

	do {
		clrscr();
		print_cash_receipt_menu();

		int status_code = read_int(buff, &end, cash_receipt_option);
		if (status_code != 0)
		{
			//printf("Error while raeding cash receipt option. \n");
			return status_code;
		}
	} while (end != buff + strlen(buff)
				|| buff[0] == 0 || *cash_receipt_option < 1 || *cash_receipt_option > 2);

	return 0;
}

int read_cash_receipt(int socket, char *buff)
{
	if (read(socket, buff, MAX_BUFF_SIZE) < 0)
	{
		perror("Error while reading cash receipt from bank.");
		return -1009;
	}

	return 0;
}

int read_int(char *buff, char **end, int *value)
{
    if (!fgets(buff, MAX_BUFF_SIZE, stdin))
    {
        perror("Error while reading from stdin. Reason ");
        return -1005;
    }

    buff[strlen(buff) - 1] = 0;

    *value = strtol(buff, &(*end), 10);

    return 0;
}

void send_response_status_msg(int socket, char *msg)
{
    //printf("Sending response msg: %s\n", msg); 
    if (write(socket, msg, strlen(msg) + 1) != strlen(msg) + 1)
    {   
        perror("Error while sending response to atm. Reason ");
    }
}

void print_languages_menu()
{
    printf("Please select a language: \n");
    printf("1. English\n");
    printf("2. Български\n");
}

void print_sums_menu()
{
    switch (current_language)
    {
        case LANG_EN:
            print_sums_menu_en();
            break;
        case LANG_BG:
            print_sums_menu_bg();
            break;
        default:
            print_sums_menu_en();
            break;
    }
}

void print_sums_menu_en()
{
    printf("Choose sum to withdraw: \n");
    printf("1. 10 BGN\n");
    printf("2. 20 BGN\n");
    printf("3. 30 BGN\n");
    printf("4. 50 BGN\n");
    printf("5. 100 BGN\n");
    printf("6. Other sum\n");
}

void print_sums_menu_bg()
{
    printf("Изберете сума за теглене: \n");
    printf("1. 10 ЛВ\n");
    printf("2. 20 ЛВ\n");
    printf("3. 30 ЛВ\n");
    printf("4. 50 ЛВ\n");
    printf("5. 100 ЛВ\n");
    printf("6. Друга сума\n");
}

void print_custom_withdraw_menu()
{
    switch (current_language)
    {
        case LANG_EN:
            print_custom_withdraw_menu_en();
            break;
        case LANG_BG:
            print_custom_withdraw_menu_bg();
            break;
        default:
            print_custom_withdraw_menu_en();
            break;
    }
}

void print_custom_withdraw_menu_en()
{
	printf("Min sum for withdraw: %d, Max sum for withdraw: %d\n", MIN_WITHDRAW_LIMIT, MAX_WITHDRAW_LIMIT);
	printf("Allowed banknote: 10, 20, 50, 100 \n");
	printf("Please input sum for withdraw: \n");
}

void print_custom_withdraw_menu_bg()
{
	printf("Минимална сума за теглене: %d, Максимална сума за теглене: %d\n", MIN_WITHDRAW_LIMIT, MAX_WITHDRAW_LIMIT);
	printf("Позволени банкноти: 10, 20, 50, 100 \n");
	printf("Моля въведете сума за теглене: \n");
}

void print_pin_code_menu()
{
    switch (current_language)
    {
        case LANG_EN:
            print_pin_code_menu_en();
            break;
        case LANG_BG:
            print_pin_code_menu_bg();
            break;
        default:
            print_pin_code_menu_en();
            break;
    }
}

void print_pin_code_menu_en()
{
	printf("Please enter your PIN code: \n");
}

void print_pin_code_menu_bg()
{
	printf("Моля въведи своя ПИН код: \n");
}

void print_invalid_pin_code_msg(int attempt_number)
{
    switch (current_language)
    {
        case LANG_EN:
            print_invalid_pin_code_msg_en(attempt_number);
            break;
        case LANG_BG:
            print_invalid_pin_code_msg_bg(attempt_number);
            break;
        default:
            print_invalid_pin_code_msg_en(attempt_number);
            break;
    }
}

void print_invalid_pin_code_msg_en(int attempt_number)
{
	printf("Invalid PIN code! Attempt %d of %d\n", attempt_number, MAX_PIN_CODE_ENTRIES);

	if (attempt_number == MAX_PIN_CODE_ENTRIES)
	{
		printf("Max attempts reached. Aborting transaction...\n");
	}
}

void print_invalid_pin_code_msg_bg(int attempt_number)
{  
    printf("Невалиден ПИН код! Опит %d от %d\n", attempt_number, MAX_PIN_CODE_ENTRIES);

    if (attempt_number == MAX_PIN_CODE_ENTRIES)
    {
        printf("Достигнат е максималния брой опити. Прекратяване на транзакцията...\n");
    }
}

void print_balance_error_msg()
{
    switch (current_language)
    {
        case LANG_EN:
            print_balance_error_msg_en();
            break;
        case LANG_BG:
            print_balance_error_msg_bg();
            break;
        default:
            print_balance_error_msg_en();
            break;
    }
}

void print_balance_error_msg_en()
{
	printf("Not enough balance, aborting transaction...\n");
}

void print_balance_error_msg_bg()
{
	printf("Недостатъчна наличност, прекратяване на транзакцията...\n");
}

void print_cash_receipt_menu()
{
    switch (current_language)
    {
        case LANG_EN:
            print_cash_receipt_menu_en();
            break;
        case LANG_BG:
            print_cash_receipt_menu_bg();
            break;
        default:
            print_cash_receipt_menu_en();
            break;
    }
}

void print_cash_receipt_menu_en()
{
	printf("Would you like a cash receipt ?\n");
	printf("1. Yes\n");
	printf("2. No\n");
}

void print_cash_receipt_menu_bg()
{
	printf("Искате ли разписка ?\n");
	printf("1. Да\n");
	printf("2. Не\n");
}

void print_cash_receipt(char *buff)
{
	char withdraw_sum[20];
	char balance[20];
	char delim[] = "|";

	char *ptr = strtok(buff, delim);
	strcpy(withdraw_sum, ptr);
	ptr = strtok(NULL, delim);
	strcpy(balance, ptr);

	time_t now;
	time(&now);
	struct tm *local = localtime(&now);

	char timestamp[MAX_BUFF_SIZE];
	sprintf(timestamp, "%02d-%02d-%d %02d:%02d:%02d", local->tm_mday, local->tm_mon + 1,
		local->tm_year + 1900, local->tm_hour, local->tm_min, local->tm_sec);

    switch (current_language)
    {
        case LANG_EN:
            print_cash_receipt_en(timestamp, withdraw_sum, balance);
            break;
        case LANG_BG:
            print_cash_receipt_bg(timestamp, withdraw_sum, balance);
            break;
        default:
            print_cash_receipt_en(timestamp, withdraw_sum, balance);
            break;
    }
}

void print_cash_receipt_en(char *timestamp, char *withdraw_sum, char *balance)
{
	printf("Date: %s\n", timestamp);
	printf("Withdraw sum: %s\n", withdraw_sum);
	printf("Balance: %s\n", balance);
}

void print_cash_receipt_bg(char *timestamp, char *withdraw_sum, char *balance)
{
	printf("Дата: %s\n", timestamp);
	printf("Изтеглена сума: %s\n", withdraw_sum);
	printf("Текущ баланс: %s\n", balance);
}

void print_error_msg()
{
    switch (current_language)
    {
        case LANG_EN:
			print_error_msg_en();
            break;
        case LANG_BG:
			print_error_msg_bg();
            break;
        default:
			print_error_msg_en();
            break;
    }
}

void print_error_msg_en()
{
	printf("An error occurred while processing your request. Please try again later.\n");
}

void print_error_msg_bg()
{
	printf("Възникна грешка при обработката на вашата заявка. Моля опитайте по-късно.\n");
}