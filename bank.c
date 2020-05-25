#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include "bank_strucs.h"

#define PORT 15001 
#define SA struct sockaddr
#define BACKLOG 50

#define BANK_FILE "bank.dat"
#define MAX_BUFF_SIZE 100
#define CARDS_COUNT 3
#define CARD_VALID_MSG "CARD_VALID"
#define CARD_INVALID_MSG "CARD_INVALID"
#define CARD_NO_EXIST_MSG "CARD_NO_EXIST"
#define SUM_SUCCESSFULLY_DEPOSITED_MSG "SUCCESS_DEPOSIT"
#define SUM_NOT_INTEGER_MSG "SUM_NOT_INTEGER"
#define SUM_INVALID_MSG "SUM_INVALID"
#define ACCOUNT_BALANCE_NOT_ENOUGH_MSG "ACCOUNT_BALANCE_NOT_ENOUGH"
#define FAILED_TRANSACTION_ERROR_MSG "FAILED_TRANSACTION_ERROR"
#define PROBLEM_RECEIVING_PIN_CODE_MSG "PROBLEM_RECEIVING_PIN_CODE"
#define PIN_CODE_RECEIVED_MSG "PIN_CODE_RECEIVED"
#define CASH_RECEIPT_YES_MSG "CASH_RECEIPT_YES"
#define CASH_RECEIPT_NO_MSG "CASH_RECEIPT_NO"
#define CASH_RECEIPT_INVALID_MSG "CASH_RECEIPT_NO"
#define CARD_NUMBER_LEN 16
#define MAX_BUFF_SIZE 100
#define MIN_WITHDRAW_LIMIT 10
#define MAX_WITHDRAW_LIMIT 1000

void *processRequest(void *);
int load_bank_data();
int validate_card(int socket);
void send_pin_code(int socket, sBankCards bank_card);
void receive_sum(int socket, int *sum_for_withdraw);
void validate_sum(int socket, int sum_for_withdraw);
void check_account_balance(int socket, sBankCards bank_card, int sum_for_withdraw);
void withdraw_sum(int socket, sBankCards *bank_card, int sum_for_withdraw);
int save_bank_data();
void cash_receipt(int socket, float balance, int withdraw_sum);
int write_test_data();
void send_response_status_msg(int socket, char *msg);
void terminate_connection(int socket);

sBankCards bank_cards[CARDS_COUNT];
pthread_mutex_t lock;

int main()
{
	// Invoke this func to create file "bank.dat" and fill it with test bank accounts
	if (write_test_data() != 0)
	{
		printf("There was an error while writing bank data.");
		exit(-1);
	}

	if (load_bank_data() != 0)
	{
		printf("There was an error while loading bank data.");
		exit(-2);
	}

	int server_socket, client_socket, len;
 	struct sockaddr_in server_addr;
	struct sockaddr_storage client_addr;
	pthread_t thread_ids[BACKLOG + 10];

	server_socket = socket(AF_INET, SOCK_STREAM, 0);

 	if (server_socket == -1) {
  		perror("Socket creation failed. Reason ");
  		exit(-3);
 	}
 	else
	{
  		printf("Socket successfully created..\n");
	}

	int enable_reuse = 1;
	if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char*) &enable_reuse, sizeof(int)) < 0)
	{
    	perror("Socket SO_REUSEADDR failed");
		exit(-4);
	}

	bzero(&server_addr, sizeof(server_addr));

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(PORT);

 	if ((bind(server_socket, (SA*)&server_addr, sizeof(server_addr))) != 0)
	{
  		perror("Socket bind failed. Reason ");
  		exit(-5);
 	}
 	else
	{
  		printf("Socket successfully binded..\n");
	}

 	if ((listen(server_socket, BACKLOG)) != 0)
	{
  		perror("Listen failed. Reason ");
  		exit(-6);
 	}
 	else
	{
  		printf("Server listening..\n");
	}

	pthread_t thread_id;
	while (1)
	{
		len = sizeof(client_addr);
		client_socket = accept(server_socket, (SA*)&client_addr, &len);

		if (client_socket < 0)
		{
			perror("Server acccept failed. Reason ");
			continue;
		}

       	if(pthread_create(&thread_id, NULL, processRequest, (void*) &client_socket) != 0 )
		{
        	perror("Failed to create thread. Reason ");
			continue;
		}
	}

	close(server_socket);

	return 0;
}

int load_bank_data()
{
	FILE *file;
    file = fopen(BANK_FILE, "r");
    if (file == NULL)
    {
        perror("Error opening bank accounts file. Reason ");
        return -201;
    }

	int i;
	for (i = 0; i < CARDS_COUNT; i++)
	{
    	if (fread(&bank_cards[i], sizeof(sBankCards), 1, file) != 1)
		{
			perror("Error while loading data in container. Reason ");
			fclose(file);
			return -202;
		}
        printf("Card holder: %s\n", bank_cards[i].card_holder);
        printf("Card number: %s\n", bank_cards[i].card_number);
        printf("Pin code: %s\n", bank_cards[i].pin_code);
        printf("IBAN: %s\n", bank_cards[i].account.iban);
        printf("Sum: %f\n\n", bank_cards[i].account.balance);
	}

	fclose(file);

	return 0;
}

void *processRequest (void *socket_fd)
{
	int socket = *(int*) socket_fd;
	printf("Processing ATM client request...\n");

	int card_idx = validate_card(socket);
	sBankCards *bank_card = &bank_cards[card_idx];

	printf("Card holder: %s\n", bank_card->card_holder);
	printf("Sending bank card pin code: %s\n", bank_card->pin_code);
	send_pin_code(socket, *bank_card);

	printf("Receiving sum for withdraw...\n");

	int sum_for_withdraw;

	receive_sum(socket, &sum_for_withdraw);
	validate_sum(socket, sum_for_withdraw);
	check_account_balance(socket, *bank_card, sum_for_withdraw);
	withdraw_sum(socket, bank_card, sum_for_withdraw);
	send_response_status_msg(socket, SUM_SUCCESSFULLY_DEPOSITED_MSG);
	cash_receipt(socket, bank_card->account.balance, sum_for_withdraw);

	printf("Ending connection with ATM...\n");
	terminate_connection(socket);
}

int validate_card(int socket)
{
	char card_number[CARD_NUMBER_LEN + 1];
	int bytes_read = read(socket, card_number, sizeof(card_number));
	if (bytes_read < 0)
	{
		perror("Error while reading card number: ");
		terminate_connection(socket);
	}

	printf("Card number bytes read: %d\n", bytes_read);

	if (bytes_read != CARD_NUMBER_LEN + 1 || strlen(card_number) != CARD_NUMBER_LEN)
	{
		send_response_status_msg(socket, CARD_INVALID_MSG);
		printf("Invalid card number, terminating connection with ATM...\n");
		terminate_connection(socket);
	}

	int i;
	for (i = 0; i < CARDS_COUNT; i++)
	{
		if (strcmp(bank_cards[i].card_number, card_number) == 0)
		{
			break;
		}
	}

	if (i >= CARDS_COUNT)
	{
		send_response_status_msg(socket, CARD_NO_EXIST_MSG);
		printf("Card number doesn't exist...");
		terminate_connection(socket);
	}

	printf("Card number is valid, sending confirmation to atm...\n");
	send_response_status_msg(socket, CARD_VALID_MSG);

	return i;
}

void send_pin_code(int socket, sBankCards bank_card)
{
	char buff[MAX_BUFF_SIZE];
	char receive_buff[MAX_BUFF_SIZE];

	memset(buff, 0, sizeof(buff));
	memset(receive_buff, 0, sizeof(receive_buff));

	strcpy(buff, bank_card.pin_code);

	while (strcmp(receive_buff, PIN_CODE_RECEIVED_MSG) != 0)
	{
		memset(receive_buff, 0, sizeof(receive_buff));

		printf("Sending pin code buffer: %s\n", buff);
		if (write(socket, buff, sizeof(buff)) != sizeof(buff))
		{
			perror("Error while sending pin code to ATM. Reason ");
			terminate_connection(socket);
		}

		if (read(socket, receive_buff, sizeof(receive_buff)) < 0)
		{
			perror("Error while receiving pin code receive msg. Reason ");
			terminate_connection(socket);
		}
	}
}

void receive_sum(int socket, int *sum_for_withdraw)
{
	char *end;
	char buff[MAX_BUFF_SIZE];

	if (read(socket, buff, sizeof(buff)) < 0)
	{
		perror("Error while receiving sum from ATM. Reason ");
		terminate_connection(socket);
	}

    *sum_for_withdraw = strtol(buff, &end, 10);

	if (end != buff + strlen(buff))
	{
		printf("Error while converting sum to integer.\n");
		send_response_status_msg(socket, SUM_NOT_INTEGER_MSG);
		terminate_connection(socket);
	}

	printf("Sum received: %d\n", *sum_for_withdraw);
}

void validate_sum(int socket, int sum_for_withdraw)
{
	if (sum_for_withdraw < MIN_WITHDRAW_LIMIT
		|| sum_for_withdraw > MAX_WITHDRAW_LIMIT
		|| sum_for_withdraw % 10 != 0)
	{
		printf("Invalid sum received from ATM: %d\n", sum_for_withdraw);
		send_response_status_msg(socket, SUM_INVALID_MSG);
		terminate_connection(socket);
	}
}

void check_account_balance(int socket, sBankCards bank_card, int sum_for_withdraw)
{
	if (bank_card.account.balance < sum_for_withdraw)
	{
		printf("Account balance is not enough to finish the transaction. Balance %f\n", bank_card.account.balance);
		send_response_status_msg(socket, ACCOUNT_BALANCE_NOT_ENOUGH_MSG);
		terminate_connection(socket);
	}
}

void withdraw_sum(int socket, sBankCards *bank_card, int sum_for_withdraw)
{
	pthread_mutex_lock(&lock);

	bank_card->account.balance = bank_card->account.balance - sum_for_withdraw;

	int status_code = save_bank_data();
	if (status_code != 0)
	{
		send_response_status_msg(socket, FAILED_TRANSACTION_ERROR_MSG);
		pthread_mutex_unlock(&lock);
		terminate_connection(socket);
	}

	pthread_mutex_unlock(&lock);
}

int save_bank_data()
{
    FILE *file;

    file = fopen(BANK_FILE, "w");
    if (file == NULL)
    {
        perror("Error while opening bank file. Reason ");
        return -1001;
    }

    int i;
    for (i = 0; i < CARDS_COUNT; i++ )
    {
        if (fwrite(&bank_cards[i], sizeof(sBankCards), 1, file) < 1)
        {
            perror("Error while writing bank card data. Reason ");
            fclose(file);
            return -1002;
        }
    }

    fclose(file);

	return 0;
}

void cash_receipt(int socket, float balance, int withdraw_sum)
{
	char buff[MAX_BUFF_SIZE];

	printf("Receiving cash receipt answer...\n");

	if (read(socket, buff, sizeof(buff)) < 0)
	{
		perror("Error while receiving cash receipt answer. Reason ");
		terminate_connection(socket);
	}

	printf("Cash receipt answer: %s\n", buff);

	if (strcmp(buff, CASH_RECEIPT_YES_MSG) == 0)
	{
		printf("Sending cash receipt to ATM...\n");

		memset(buff, 0, sizeof(buff));
		sprintf(buff, "%d|%.2f", withdraw_sum, balance);

		if (write(socket, buff, sizeof(buff)) != sizeof(buff))
		{
			perror("Error while sending cash receipt. Reason ");
			terminate_connection(socket);
		}
	}
}

void send_response_status_msg(int socket, char *msg)
{
	printf("Sending response msg: %s\n", msg);
	if (write(socket, msg, strlen(msg) + 1) != strlen(msg) + 1)
	{
		perror("Error while sending response to atm. Reason ");
	}
}

void terminate_connection(int socket)
{
	close(socket);
	pthread_exit(NULL);
}


int write_test_data()
{
	sBankCards bank_cards[CARDS_COUNT] =
	{
		{"Georgi Georgiev", "4169910013514509", "0000", "BG69STSA93000023280109", 1000.00},
		{"Ivan Ivanov", "4167910013514510", "1111", "BG69STSA93000023280110", 786.19},
		{"Petar Petrov", "4179910013514610", "2323", "BG69STSA93000023280111", 125.01}
	};

	FILE *file;

	file = fopen(BANK_FILE, "w");
	if (file == NULL)
	{
		perror("Error while writing bank data. Reason ");
		return -101;
	}

	int i;
	for (i = 0; i < CARDS_COUNT; i++ )
	{
		if (fwrite(&bank_cards[i], sizeof(sBankCards), 1, file) < 1)
		{
			perror("Error while writing bank card data. Reason ");
			fclose(file);
			return -202;
		}
	}

	fclose(file);

	return 0;
}