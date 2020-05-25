#ifndef BANK_STRUCS
#define BANK_STRUCS

typedef struct BankAccount
{
    char iban[23];
    double balance;
} sBankAccounts;

typedef struct BankCard
{
    char card_holder[100];
    char card_number[17];
    char pin_code[5];
    sBankAccounts account;
} sBankCards;

#endif

#define clrscr() printf("\e[1;1H\e[2J")
