#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> // Added for case-insensitive search

#define MAX_BOOKS 100
#define FILE_NAME "library_data.dat"

// Struct representing a Library Book
typedef struct {
    int bookID;
    char title[100];
    char author[50];
    int isIssued; // 0 = available, 1 = issued
    char studentRegNo[20];
} Book;

// Global variables to store books in memory
Book library[MAX_BOOKS];
int bookCount = 0;

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

// This asynchronous JS function polls the `stdinBuffer` array every 50ms.
// Emscripten will suspend the C program until the Promise resolves!
EM_ASYNC_JS(void, wait_for_input, (), {
    return new Promise(function(resolve) {
        function check() {
            if (typeof stdinBuffer !== 'undefined' && stdinBuffer.length > 0) {
                resolve();
            } else {
                setTimeout(check, 50);
            }
        }
        check();
    });
});

// Wrapper for fgets to wait for HTML button inputs
char* wasm_fgets(char* str, int n, FILE* stream) {
    if (stream == stdin) {
        clearerr(stdin); // Reset EOF state from previous empty reads just in case
        wait_for_input(); // Yield to browser until user hits SEND
    }
    return fgets(str, n, stream);
}
#define fgets wasm_fgets
#endif

// Function prototypes
void loadRecords();
void saveRecords();
void addBook();
void issueBook();
void returnBook();
void searchBook();
void displayIssuedBooks();
void displayAllBooks(); // NEW FEATURE added
void printMenu();

// New UI / Table rendering prototypes
void printTableHeader();
void printBookTableRow(Book book);

// Enhanced Input Validation Prototypes
void stripNewline(char* str);
int getValidBookID(const char* prompt);
void getValidString(const char* prompt, char* output, int maxLen);
int containsCaseInsensitive(const char* haystack, const char* needle);

int main() {
    /* 
     * CRITICAL FOR EMSCRIPTEN: 
     * Disable output buffering so `printf` statements show up immediately 
     * on the HTML console before the browser pauses for `fgets` input. 
     */
    setbuf(stdout, NULL);
    
    printf("==========================================\n");
    printf("   Library Book Tracking System (WASM)    \n");
    printf("==========================================\n");
    fflush(stdout); // explicitly flush for UI consistency
    
    loadRecords();
    
    char inputBuffer[20];
    int choice = 0;

    while (1) {
        printMenu();
        printf("Enter your choice: ");
        fflush(stdout);
        
        if (fgets(inputBuffer, sizeof(inputBuffer), stdin) != NULL) {
            if (sscanf(inputBuffer, "%d", &choice) != 1) {
                choice = 0; // Ensures bad input hits the default failure block
            }
        } else {
            break; 
        }
        
        switch (choice) {
            case 1: addBook(); break;
            case 2: issueBook(); break;
            case 3: returnBook(); break;
            case 4: searchBook(); break;
            case 5: displayIssuedBooks(); break;
            case 6: displayAllBooks(); break;
            case 7: 
                saveRecords();
                printf("\nExiting program. Goodbye!\n");
                return 0;
            default:
                printf("\n[!] Invalid choice. Please enter a number between 1 and 7.\n");
        }
    }
    return 0;
}

// ---------------------------------------------------------
// Helper: Removes trailing newline left by fgets
// ---------------------------------------------------------
void stripNewline(char* str) {
    size_t len = strlen(str);
    if (len > 0 && str[len - 1] == '\n') {
        str[len - 1] = '\0';
    }
}

// ---------------------------------------------------------
// NEW HELPER: Case-insensitive substring search (Improvement #2)
// ---------------------------------------------------------
int containsCaseInsensitive(const char* haystack, const char* needle) {
    char tempHaystack[256], tempNeedle[256];
    
    // Copy safely
    strncpy(tempHaystack, haystack, sizeof(tempHaystack) - 1);
    tempHaystack[sizeof(tempHaystack) - 1] = '\0';
    
    strncpy(tempNeedle, needle, sizeof(tempNeedle) - 1);
    tempNeedle[sizeof(tempNeedle) - 1] = '\0';

    // Convert both to lowercase
    for (int i = 0; tempHaystack[i]; i++) {
        tempHaystack[i] = tolower((unsigned char)tempHaystack[i]);
    }
    for (int i = 0; tempNeedle[i]; i++) {
        tempNeedle[i] = tolower((unsigned char)tempNeedle[i]);
    }

    return strstr(tempHaystack, tempNeedle) != NULL;
}

// ---------------------------------------------------------
// NEW HELPER: Guaranteed valid positive integer ID  (Improvement #1 & #4)
// ---------------------------------------------------------
int getValidBookID(const char* prompt) {
    char buffer[256];
    int id;
    while (1) {
        printf("%s", prompt);
        fflush(stdout);
        if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
            stripNewline(buffer);
            if (strlen(buffer) == 0) continue; // Ignore empty presses

            // sscanf safely parses ensuring numeric type
            if (sscanf(buffer, "%d", &id) != 1) {
                printf("[!] Invalid numeric Book ID.\n");
            } else if (id < 0) {
                printf("[!] Book ID cannot be negative.\n");
            } else {
                return id; // Fully valid and clean
            }
        } else {
            return -1; // Fallback for EOF
        }
    }
}

// ---------------------------------------------------------
// NEW HELPER: Prevents totally empty strings (Improvement #4)
// ---------------------------------------------------------
void getValidString(const char* prompt, char* output, int maxLen) {
    while (1) {
        printf("%s", prompt);
        fflush(stdout);
        if (fgets(output, maxLen, stdin) != NULL) {
            stripNewline(output);
            if (strlen(output) > 0) {
                break;
            }
        }
        printf("[!] Input cannot be empty. Please try again.\n");
    }
}

// ---------------------------------------------------------
// Displays the main menu options (Moved Save to 7)
// ---------------------------------------------------------
void printMenu() {
    printf("\n--- Main Menu ---\n");
    printf("1. Add a Book\n");
    printf("2. Issue a Book\n");
    printf("3. Return a Book\n");
    printf("4. Search Book (by ID or Title)\n");
    printf("5. Display Issued Books\n");
    printf("6. Display All Books\n");
    printf("7. Save & Exit\n");
}

// ---------------------------------------------------------
// Book Table Rendering Helpers (Improvement #5 & #3)
// ---------------------------------------------------------
void printTableHeader() {
    printf("%-10s | %-30s | %-20s | %-20s\n", "Book ID", "Title", "Author", "Status");
    printf("----------------------------------------------------------------------------------------\n");
}

void printBookTableRow(Book book) {
    char status[50];
    if (book.isIssued) {
        sprintf(status, "Issued (%s)", book.studentRegNo); // Custom status format
    } else {
        strcpy(status, "Available");
    }
    // Formatted output aligning nicely
    printf("%-10d | %-30.30s | %-20.20s | %-20s\n", 
           book.bookID, book.title, book.author, status);
}

// ---------------------------------------------------------
// File handling: Load records from Emscripten Virtual FS
// ---------------------------------------------------------
void loadRecords() {
    FILE *file = fopen(FILE_NAME, "rb");
    if (file == NULL) {
        printf("[i] No existing records found. Starting a fresh library.\n");
        return;
    }
    fread(&bookCount, sizeof(int), 1, file);
    fread(library, sizeof(Book), bookCount, file);
    fclose(file);
    printf("[i] Successfully loaded %d books from file memory.\n", bookCount);
}

// ---------------------------------------------------------
// File handling: Save records to Emscripten Virtual FS (MEMFS)
// ---------------------------------------------------------
void saveRecords() {
    FILE *file = fopen(FILE_NAME, "wb");
    if (file == NULL) {
        printf("[!] Error: Could not save records to file!\n");
        return;
    }
    fwrite(&bookCount, sizeof(int), 1, file);
    fwrite(library, sizeof(Book), bookCount, file);
    fclose(file);
    printf("[i] Successfully saved %d books to file memory.\n", bookCount);
}

// ---------------------------------------------------------
// Feature 1: Add a Book to the library
// ---------------------------------------------------------
void addBook() {
    printf("\n--- Add a New Book ---\n");
    if (bookCount >= MAX_BOOKS) {
        printf("[!] Library is full. Cannot add more books.\n");
        return;
    }

    Book newBook;
    
    // Guarantee non-duplicate IDs using our new validation helper
    while (1) {
        newBook.bookID = getValidBookID("Enter Book ID (numeric): ");
        if (newBook.bookID == -1) return; 
        
        int duplicate = 0;
        for (int i = 0; i < bookCount; i++) {
            if (library[i].bookID == newBook.bookID) {
                printf("[!] Error: A book with ID %d already exists.\n", newBook.bookID);
                duplicate = 1;
                break;
            }
        }
        if (!duplicate) break; 
    }

    // Force strict length inputs instead of accepting blanks
    getValidString("Enter Title: ", newBook.title, sizeof(newBook.title));
    getValidString("Enter Author: ", newBook.author, sizeof(newBook.author));

    newBook.isIssued = 0; 
    strcpy(newBook.studentRegNo, ""); 

    library[bookCount] = newBook;
    bookCount++;

    printf("[+] Book '%s' added successfully!\n", newBook.title);
}

// ---------------------------------------------------------
// Feature 2: Issue a Book to a student
// ---------------------------------------------------------
void issueBook() {
    printf("\n--- Issue a Book ---\n");
    int searchID = getValidBookID("Enter Book ID to issue: ");
    if (searchID == -1) return;

    for (int i = 0; i < bookCount; i++) {
        if (library[i].bookID == searchID) {
            if (library[i].isIssued == 1) {
                printf("[!] Sorry, this book is already issued to student: %s\n", library[i].studentRegNo);
                return;
            }
            
            getValidString("Enter Student Registration Number: ", library[i].studentRegNo, sizeof(library[i].studentRegNo));
            library[i].isIssued = 1;
            printf("[+] Success! Book ID %d has been issued to %s.\n", searchID, library[i].studentRegNo);
            return;
        }
    }
    printf("[!] Error: Book with ID %d not found in the library.\n", searchID);
}

// ---------------------------------------------------------
// Feature 3: Return a Book
// ---------------------------------------------------------
void returnBook() {
    printf("\n--- Return a Book ---\n");
    int searchID = getValidBookID("Enter Book ID to return: ");
    if (searchID == -1) return;

    for (int i = 0; i < bookCount; i++) {
        if (library[i].bookID == searchID) {
            if (library[i].isIssued == 0) {
                printf("[!] This book is already available in the library.\n");
                return;
            }
            
            library[i].isIssued = 0;
            strcpy(library[i].studentRegNo, ""); 
            printf("[+] Success! Book ID %d has been returned to the library.\n", searchID);
            return;
        }
    }
    printf("[!] Error: Book with ID %d not found.\n", searchID);
}

// ---------------------------------------------------------
// Feature 4: Search Book by ID or Title
// ---------------------------------------------------------
void searchBook() {
    printf("\n--- Search Book ---\n");
    char buffer[256];
    int option = 0;
    
    // Validate Sub-Menu Selection
    while (1) {
        printf("Search by (1) Book ID or (2) Title Keyword? Enter 1 or 2: ");
        fflush(stdout);
        if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
            if (sscanf(buffer, "%d", &option) == 1 && (option == 1 || option == 2)) {
                break;
            }
        } else {
            return;
        }
        printf("[!] Invalid choice. Please enter 1 or 2.\n");
    }

    if (option == 1) {
        int searchID = getValidBookID("Enter Book ID: ");
        if (searchID == -1) return;
        
        for (int i = 0; i < bookCount; i++) {
            if (library[i].bookID == searchID) {
                printf("\n--- Book Found ---\n");
                printTableHeader();
                printBookTableRow(library[i]);
                return;
            }
        }
        printf("[!] No book found with ID %d.\n", searchID);
    } 
    else if (option == 2) {
        char keyword[100];
        getValidString("Enter title keyword: ", keyword, sizeof(keyword));
        
        int foundCount = 0;
        printf("\n--- Search Results ---\n");
        for (int i = 0; i < bookCount; i++) {
            // Using newest case-insensitive validation
            if (containsCaseInsensitive(library[i].title, keyword)) {
                if (foundCount == 0) printTableHeader(); // Draw header once
                printBookTableRow(library[i]);
                foundCount++;
            }
        }
        if (foundCount == 0) {
            printf("[!] No books found matching keyword '%s'.\n", keyword);
        }
    }
}

// ---------------------------------------------------------
// Feature 5: Display Issued Books
// ---------------------------------------------------------
void displayIssuedBooks() {
    printf("\n--- Currently Issued Books ---\n");
    int foundCount = 0;
    
    for (int i = 0; i < bookCount; i++) {
        if (library[i].isIssued == 1) {
            if (foundCount == 0) printTableHeader();
            printBookTableRow(library[i]);
            foundCount++;
        }
    }
    if (foundCount == 0) {
        printf("[!] No books are currently issued out.\n");
    }
}

// ---------------------------------------------------------
// NEW FEATURE 6: Display All Books 
// ---------------------------------------------------------
void displayAllBooks() {
    printf("\n--- All Books in Library ---\n");
    if (bookCount == 0) {
        printf("[i] The library is currently empty.\n");
        return;
    }
    
    printTableHeader();
    for (int i = 0; i < bookCount; i++) {
        printBookTableRow(library[i]);
    }
}
