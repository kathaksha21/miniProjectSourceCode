/*
 * TransC Bank System
 *
 * This reduced implementation preserves the core banking features
 * requested by the user while keeping the code compact and readable.
 *
 * Included features:
 *   - account authentication with per-account passwords
 *   - login locking after repeated failed attempts
 *   - deposit and withdrawal support
 *   - money transfer between accounts
 *   - transaction history logging and history display
 *   - account creation and active account listing
 *   - simple export of account listings to text file
 *
 * This file is written to maintain the banking behavior while reducing
 * unnecessary comment noise and repetitive spacing from the longer
 * version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#define MAX_ACCOUNTS 100
#define NAME_LEN 15
#define FIRST_LEN 10
#define PASS_LEN 12
#define MEMO_LEN 64
#define ACCOUNT_FILE "accounts.dat"
#define TRANSACTION_FILE "transactions.dat"
#define EXPORT_FILE "accounts.txt"
#define MAX_FAILED_LOGINS 3

typedef struct {
    unsigned acctNum;
    char lastName[NAME_LEN];
    char firstName[FIRST_LEN];
    char password[PASS_LEN];
    double balance;
    bool locked;
    unsigned failedAttempts;
    time_t createdAt;
} Account;

typedef struct {
    unsigned sourceAcct;
    unsigned targetAcct;
    char type;
    double amount;
    double balanceAfter;
    time_t timestamp;
    char memo[MEMO_LEN];
} Transaction;

static void clearInput(void)
{
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF) {
    }
}

static void ensureDatabase(void)
{
    FILE *accountFile = fopen(ACCOUNT_FILE, "rb");
    if (accountFile == NULL) {
        accountFile = fopen(ACCOUNT_FILE, "wb");
        if (accountFile == NULL) {
            fprintf(stderr, "Cannot create %s\n", ACCOUNT_FILE);
            exit(EXIT_FAILURE);
        }
        Account blank = {0};
        for (unsigned i = 0; i < MAX_ACCOUNTS; ++i) {
            fwrite(&blank, sizeof(Account), 1, accountFile);
        }
    }
    if (accountFile != NULL) {
        fclose(accountFile);
    }
    FILE *txFile = fopen(TRANSACTION_FILE, "ab+");
    if (txFile == NULL) {
        fprintf(stderr, "Cannot create %s\n", TRANSACTION_FILE);
        exit(EXIT_FAILURE);
    }
    fclose(txFile);
}

static bool loadAccount(FILE *accountFile, unsigned acctNum, Account *account)
{
    if (acctNum == 0 || acctNum > MAX_ACCOUNTS) {
        return false;
    }
    fseek(accountFile, (long)(acctNum - 1) * sizeof(Account), SEEK_SET);
    if (fread(account, sizeof(Account), 1, accountFile) != 1) {
        return false;
    }
    return account->acctNum == acctNum;
}

static void saveAccount(FILE *accountFile, const Account *account)
{
    fseek(accountFile, (long)(account->acctNum - 1) * sizeof(Account), SEEK_SET);
    fwrite(account, sizeof(Account), 1, accountFile);
    fflush(accountFile);
}

static unsigned readUnsigned(const char *prompt)
{
    unsigned value = 0;
    printf("%s", prompt);
    if (scanf("%u", &value) != 1) {
        clearInput();
        return 0;
    }
    clearInput();
    return value;
}

static void readPassword(char *password)
{
    printf("Password: ");
    scanf("%11s", password);
    clearInput();
}

static void stampTime(time_t timestamp, char buffer[32])
{
    struct tm *tmInfo = localtime(&timestamp);
    if (tmInfo == NULL) {
        strncpy(buffer, "unknown", 32);
        buffer[31] = '\0';
        return;
    }
    strftime(buffer, 32, "%Y-%m-%d %H:%M:%S", tmInfo);
}

static void logTransaction(FILE *txFile, const Transaction *tx)
{
    fseek(txFile, 0, SEEK_END);
    fwrite(tx, sizeof(Transaction), 1, txFile);
    fflush(txFile);
}

static void listAccounts(FILE *accountFile)
{
    Account account = {0};
    rewind(accountFile);
    printf("\nActive accounts:\n");
    printf("%-5s %-14s %-12s %10s\n", "Acct", "Last", "First", "Balance");
    printf("%-5s %-14s %-12s %10s\n", "----", "--------------", "------------", "----------");
    for (unsigned i = 0; i < MAX_ACCOUNTS; ++i) {
        fread(&account, sizeof(Account), 1, accountFile);
        if (account.acctNum != 0) {
            printf("%-5u %-14s %-12s %10.2f\n",
                   account.acctNum,
                   account.lastName,
                   account.firstName,
                   account.balance);
        }
    }
}

static void exportAccounts(FILE *accountFile)
{
    FILE *outFile = fopen(EXPORT_FILE, "w");
    if (outFile == NULL) {
        printf("Unable to write %s\n", EXPORT_FILE);
        return;
    }
    Account account = {0};
    rewind(accountFile);
    fprintf(outFile, "%-5s %-14s %-12s %10s %20s\n", "Acct", "Last", "First", "Balance", "Created");
    fprintf(outFile, "%-5s %-14s %-12s %10s %20s\n", "----", "--------------", "------------", "----------", "--------------------");
    for (unsigned i = 0; i < MAX_ACCOUNTS; ++i) {
        fread(&account, sizeof(Account), 1, accountFile);
        if (account.acctNum != 0) {
            char timebuf[32];
            stampTime(account.createdAt, timebuf);
            fprintf(outFile, "%-5u %-14s %-12s %10.2f %20s\n",
                    account.acctNum,
                    account.lastName,
                    account.firstName,
                    account.balance,
                    timebuf);
        }
    }
    fclose(outFile);
    printf("Accounts exported to %s\n", EXPORT_FILE);
}

static void showAccountSummary(const Account *account)
{
    char timebuf[32];
    stampTime(account->createdAt, timebuf);
    printf("\nAccount summary for %u:\n", account->acctNum);
    printf("  Name          : %s %s\n", account->firstName, account->lastName);
    printf("  Balance       : $%.2f\n", account->balance);
    printf("  Created       : %s\n", timebuf);
    printf("  Locked        : %s\n", account->locked ? "yes" : "no");
}

static void showHistory(FILE *txFile, unsigned acctNum)
{
    Transaction tx = {0};
    unsigned count = 0;
    rewind(txFile);
    printf("\nTransaction history for account %u:\n", acctNum);
    printf("%-19s %-12s %10s %10s %s\n", "Date", "Type", "Amount", "Balance", "Memo");
    while (fread(&tx, sizeof(Transaction), 1, txFile) == 1) {
        if (tx.sourceAcct == acctNum || tx.targetAcct == acctNum) {
            char timebuf[32];
            stampTime(tx.timestamp, timebuf);
            const char *typeLabel = "Unknown";
            if (tx.type == 'D') {
                typeLabel = "Deposit";
            } else if (tx.type == 'W') {
                typeLabel = "Withdrawal";
            } else if (tx.type == 'T') {
                typeLabel = "Transfer out";
            } else if (tx.type == 'R') {
                typeLabel = "Transfer in";
            }
            printf("%-19s %-12s %10.2f %10.2f %s\n",
                   timebuf,
                   typeLabel,
                   tx.amount,
                   tx.balanceAfter,
                   tx.memo);
            ++count;
        }
    }
    if (count == 0) {
        printf("No transactions found.\n");
    }
}

static void changePassword(FILE *accountFile, Account *account)
{
    char current[PASS_LEN] = "";
    char replacement[PASS_LEN] = "";
    printf("\nEnter current password:\n");
    readPassword(current);
    if (strncmp(current, account->password, PASS_LEN) != 0) {
        printf("Incorrect password.\n");
        return;
    }
    printf("Enter new password:\n");
    readPassword(replacement);
    if (replacement[0] == '\0') {
        printf("Password cannot be empty.\n");
        return;
    }
    strncpy(account->password, replacement, PASS_LEN - 1);
    account->password[PASS_LEN - 1] = '\0';
    saveAccount(accountFile, account);
    printf("Password changed successfully.\n");
}

static void depositFunds(FILE *accountFile, FILE *txFile, Account *account)
{
    double amount = 0.0;
    printf("\nEnter deposit amount:\n");
    if (scanf("%lf", &amount) != 1) {
        clearInput();
        printf("Invalid amount.\n");
        return;
    }
    clearInput();
    if (amount <= 0.0) {
        printf("Deposit amount must be positive.\n");
        return;
    }
    account->balance += amount;
    saveAccount(accountFile, account);
    Transaction tx = {account->acctNum, 0, 'D', amount, account->balance, time(NULL), "Deposit"};
    logTransaction(txFile, &tx);
    printf("Deposit complete. Current balance: $%.2f\n", account->balance);
}

static void withdrawFunds(FILE *accountFile, FILE *txFile, Account *account)
{
    double amount = 0.0;
    printf("\nEnter withdrawal amount:\n");
    if (scanf("%lf", &amount) != 1) {
        clearInput();
        printf("Invalid amount.\n");
        return;
    }
    clearInput();
    if (amount <= 0.0) {
        printf("Withdrawal amount must be positive.\n");
        return;
    }
    if (amount > account->balance) {
        printf("Insufficient funds. Current balance: $%.2f\n", account->balance);
        return;
    }
    account->balance -= amount;
    saveAccount(accountFile, account);
    Transaction tx = {account->acctNum, 0, 'W', amount, account->balance, time(NULL), "Withdrawal"};
    logTransaction(txFile, &tx);
    printf("Withdrawal complete. Remaining balance: $%.2f\n", account->balance);
}

static void transferFunds(FILE *accountFile, FILE *txFile, Account *source)
{
    unsigned destination = readUnsigned("\nEnter destination account number:\n");
    if (destination == 0 || destination > MAX_ACCOUNTS || destination == source->acctNum) {
        printf("Invalid destination account.\n");
        return;
    }
    Account target = {0};
    if (!loadAccount(accountFile, destination, &target)) {
        printf("Destination account does not exist.\n");
        return;
    }
    double amount = 0.0;
    printf("Enter transfer amount:\n");
    if (scanf("%lf", &amount) != 1) {
        clearInput();
        printf("Invalid amount.\n");
        return;
    }
    clearInput();
    if (amount <= 0.0) {
        printf("Transfer amount must be positive.\n");
        return;
    }
    if (amount > source->balance) {
        printf("Insufficient funds. Current balance: $%.2f\n", source->balance);
        return;
    }
    source->balance -= amount;
    target.balance += amount;
    saveAccount(accountFile, source);
    saveAccount(accountFile, &target);
    Transaction outgoing = {source->acctNum, target.acctNum, 'T', amount, source->balance, time(NULL), "Transfer out"};
    Transaction incoming = {target.acctNum, source->acctNum, 'R', amount, target.balance, time(NULL), "Transfer in"};
    logTransaction(txFile, &outgoing);
    logTransaction(txFile, &incoming);
    printf("Transferred $%.2f from %u to %u. New balance: $%.2f\n",
           amount,
           source->acctNum,
           target.acctNum,
           source->balance);
}

static void accountDashboard(FILE *accountFile, FILE *txFile, Account *account)
{
    unsigned choice = 0;
    while (1) {
        printf("\n=== Account Dashboard for %s %s (Acct %u) ===\n",
               account->firstName,
               account->lastName,
               account->acctNum);
        printf("1 - Account summary\n");
        printf("2 - Deposit funds\n");
        printf("3 - Withdraw funds\n");
        printf("4 - Transfer funds\n");
        printf("5 - View transaction history\n");
        printf("6 - Change password\n");
        printf("7 - Log out\n");
        printf("Select an option:\n");
        if (scanf("%u", &choice) != 1) {
            clearInput();
            printf("Invalid selection.\n");
            continue;
        }
        clearInput();
        switch (choice) {
            case 1:
                showAccountSummary(account);
                break;
            case 2:
                depositFunds(accountFile, txFile, account);
                break;
            case 3:
                withdrawFunds(accountFile, txFile, account);
                break;
            case 4:
                transferFunds(accountFile, txFile, account);
                break;
            case 5:
                showHistory(txFile, account->acctNum);
                break;
            case 6:
                changePassword(accountFile, account);
                break;
            case 7:
                printf("Logging out.\n");
                return;
            default:
                printf("Please choose a valid option.\n");
                break;
        }
    }
}

static void createAccount(FILE *accountFile)
{
    Account account = {0};
    unsigned acctNum = readUnsigned("\nEnter new account number (1 - 100):\n");
    if (acctNum == 0 || acctNum > MAX_ACCOUNTS) {
        printf("Invalid account number.\n");
        return;
    }
    if (loadAccount(accountFile, acctNum, &account)) {
        printf("Account %u already exists.\n", acctNum);
        return;
    }
    printf("Enter last name:\n");
    scanf("%14s", account.lastName);
    clearInput();
    printf("Enter first name:\n");
    scanf("%9s", account.firstName);
    clearInput();
    printf("Enter initial balance:\n");
    if (scanf("%lf", &account.balance) != 1) {
        clearInput();
        printf("Invalid balance.\n");
        return;
    }
    clearInput();
    printf("Choose a password:\n");
    scanf("%11s", account.password);
    clearInput();
    account.acctNum = acctNum;
    account.createdAt = time(NULL);
    account.locked = false;
    account.failedAttempts = 0;
    saveAccount(accountFile, &account);
    printf("Account %u created successfully.\n", acctNum);
}

static void loginSession(FILE *accountFile, FILE *txFile)
{
    unsigned acctNum = readUnsigned("\nEnter account number:\n");
    if (acctNum == 0) {
        return;
    }
    Account account = {0};
    if (!loadAccount(accountFile, acctNum, &account)) {
        printf("Account %u does not exist.\n", acctNum);
        return;
    }
    if (account.locked) {
        printf("Account %u is locked due to failed login attempts.\n", acctNum);
        return;
    }
    char password[PASS_LEN] = "";
    readPassword(password);
    if (strncmp(password, account.password, PASS_LEN) != 0) {
        account.failedAttempts += 1;
        if (account.failedAttempts >= MAX_FAILED_LOGINS) {
            account.locked = true;
            printf("Account locked after %u failed attempts.\n", account.failedAttempts);
        } else {
            printf("Incorrect password. Attempt %u of %u.\n",
                   account.failedAttempts,
                   MAX_FAILED_LOGINS);
        }
        saveAccount(accountFile, &account);
        return;
    }
    account.failedAttempts = 0;
    saveAccount(accountFile, &account);
    printf("Login successful. Welcome, %s %s!\n", account.firstName, account.lastName);
    accountDashboard(accountFile, txFile, &account);
}

int main(void)
{
    ensureDatabase();
    FILE *accountFile = fopen(ACCOUNT_FILE, "rb+");
    if (accountFile == NULL) {
        fprintf(stderr, "Unable to open %s\n", ACCOUNT_FILE);
        return EXIT_FAILURE;
    }
    FILE *txFile = fopen(TRANSACTION_FILE, "ab+");
    if (txFile == NULL) {
        fprintf(stderr, "Unable to open %s\n", TRANSACTION_FILE);
        fclose(accountFile);
        return EXIT_FAILURE;
    }
    unsigned option = 0;
    while (true) {
        printf("\n=== TransC Bank Menu ===\n");
        printf("1 - Create account\n");
        printf("2 - Log in\n");
        printf("3 - List active accounts\n");
        printf("4 - Export accounts\n");
        printf("5 - Exit\n");
        printf("Select an option:\n");
        if (scanf("%u", &option) != 1) {
            clearInput();
            printf("Invalid input.\n");
            continue;
        }
        clearInput();
        switch (option) {
            case 1:
                createAccount(accountFile);
                break;
            case 2:
                loginSession(accountFile, txFile);
                break;
            case 3:
                listAccounts(accountFile);
                break;
            case 4:
                exportAccounts(accountFile);
                break;
            case 5:
                fclose(accountFile);
                fclose(txFile);
                printf("Goodbye!\n");
                return EXIT_SUCCESS;
            default:
                printf("Please choose a valid option.\n");
                break;
        }
    }
}
