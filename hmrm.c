/*
 * HOSTEL MAINTENANCE REQUEST MANAGER - DSA Lab (702CO0C084) Mini Project
 * One program, two roles: choose STUDENT or ADMIN (password: admin123) at start.
 * Compile: gcc hostel_maintenance.c -o hostel      Run: ./hostel
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#define MAX_REQ 100
#define CQ_SIZE 100
#define STACK_MAX 50
#define NAME_LEN 40
#define DESC_LEN 80
#define BLOCKS 6
#define REPS 2000
#define ADMIN_PASS "admin123"
#define ACT_ADD 1
#define ACT_DELETE 2
typedef struct {
    int id;
    char student[NAME_LEN];
    char block;                 /* 'A'..'E' */
    int room;
    char category[20];
    char desc[DESC_LEN];
    int priority;               /* 1 = High, 2 = Medium, 3 = Low */
    int status;                 /* 0 = Pending, 1 = Assigned, 2 = Resolved */
} Request;
static const char *STATUS_NAME[] = {"Pending", "Assigned", "Resolved"};
static const char *PRIO_NAME[] = {"", "High", "Medium", "Low"};
static const char *CATS[] = {"Plumbing", "Electrical", "Furniture", "Cleaning", "Internet", "Other"};
static const char *LOC[BLOCKS] = {"Office", "Block A", "Block B", "Block C", "Block D", "Block E"};
int adj[BLOCKS][BLOCKS] = {                         /* map: Office-A, Office-B, A-C, B-D, C-E, D-E */
    {0,1,1,0,0,0}, {1,0,0,1,0,0}, {1,0,0,0,1,0}, {0,1,0,0,0,1}, {0,0,1,0,0,1}, {0,0,0,1,1,0}
};
Request reqs[MAX_REQ];
int reqCount = 0, nextId = 1001;
typedef struct BNode { int id, idx; struct BNode *left, *right; } BNode;
BNode *root = NULL;
int bstSteps = 0;
int cq[CQ_SIZE], cqFront = 0, cqRear = 0, cqCount = 0;
typedef struct { int type; Request req; } Action;
Action undoStack[STACK_MAX];
int top = -1;
long cmpCount = 0;
char myName[NAME_LEN], myBlock;                     /* logged-in student */
int myRoom;
void readLine(const char *prompt, char *buf, int size) {
    printf("%s", prompt);
    if (!fgets(buf, size, stdin)) { printf("\nInput closed. Bye!\n"); exit(0); }
    buf[strcspn(buf, "\n")] = '\0';
}
int readInt(const char *prompt, int lo, int hi) {
    char buf[32], *end;
    while (1) {
        readLine(prompt, buf, sizeof buf);
        long v = strtol(buf, &end, 10);
        if (end != buf && *end == '\0' && v >= lo && v <= hi) return (int)v;
        printf("  Please enter a number between %d and %d.\n", lo, hi);
    }
}
char readBlock(void) {
    char buf[8];
    while (1) {
        readLine("Block (A-E): ", buf, sizeof buf);
        char c = (char)toupper((unsigned char)buf[0]);
        if (c >= 'A' && c <= 'E' && buf[1] == '\0') return c;
        printf("  Block must be one letter from A to E.\n");
    }
}
void printHeader(void) {
    printf("\n%-6s %-14s %-6s %-10s %-8s %-9s %s\n", "ID", "Student", "Room", "Category", "Priority", "Status", "Description");
    printf("--------------------------------------------------------------------------------\n");
}
void printRow(const Request *r) {
    printf("%-6d %-14.14s %c-%-4d %-10s %-8s %-9s %s\n", r->id, r->student, r->block, r->room,
           r->category, PRIO_NAME[r->priority], STATUS_NAME[r->status], r->desc);
}
BNode *bstInsert(BNode *n, int id, int idx) {
    if (!n) { n = (BNode *)malloc(sizeof(BNode)); n->id = id; n->idx = idx; n->left = n->right = NULL; }
    else if (id < n->id) n->left = bstInsert(n->left, id, idx);
    else n->right = bstInsert(n->right, id, idx);
    return n;
}
BNode *bstSearch(BNode *n, int id) {
    while (n) {
        bstSteps++;
        if (id == n->id) return n;
        n = (id < n->id) ? n->left : n->right;
    }
    return NULL;
}
BNode *bstDelete(BNode *n, int id) {
    if (!n) return NULL;
    if (id < n->id) n->left = bstDelete(n->left, id);
    else if (id > n->id) n->right = bstDelete(n->right, id);
    else {
        if (!n->left) { BNode *r = n->right; free(n); return r; }
        if (!n->right) { BNode *l = n->left; free(n); return l; }
        BNode *s = n->right;
        while (s->left) s = s->left;
        n->id = s->id; n->idx = s->idx;
        n->right = bstDelete(n->right, s->id);
    }
    return n;
}
void shiftIdx(BNode *n, int from, int delta) {
    if (!n) return;
    if (n->idx >= from) n->idx += delta;
    shiftIdx(n->left, from, delta); shiftIdx(n->right, from, delta);
}
void freeTree(BNode *n) { if (n) { freeTree(n->left); freeTree(n->right); free(n); } }
void cqEnqueue(int id) { if (cqCount < CQ_SIZE) { cq[cqRear] = id; cqRear = (cqRear + 1) % CQ_SIZE; cqCount++; } }
int cqDequeue(void) {
    if (cqCount == 0) return -1;
    int id = cq[cqFront]; cqFront = (cqFront + 1) % CQ_SIZE; cqCount--;
    return id;
}
int queuePos(int id) {                             /* 1-based position in the queue, 0 if absent */
    for (int i = 0; i < cqCount; i++) if (cq[(cqFront + i) % CQ_SIZE] == id) return i + 1;
    return 0;
}
void rebuildQueue(void) {
    cqFront = cqRear = cqCount = 0;
    for (int p = 1; p <= 3; p++)
        for (int i = 0; i < reqCount; i++) if (reqs[i].status == 0 && reqs[i].priority == p) cqEnqueue(reqs[i].id);
}
void push(int type, Request r) {
    if (top == STACK_MAX - 1) { memmove(undoStack, undoStack + 1, sizeof(Action) * (STACK_MAX - 1)); top--; }
    undoStack[++top].type = type; undoStack[top].req = r;
}
int pop(Action *a) { if (top < 0) return 0; *a = undoStack[top--]; return 1; }
int registerRequest(Request r) {
    if (reqCount >= MAX_REQ) return 0;
    int pos = 0;
    while (pos < reqCount && reqs[pos].id < r.id) pos++;
    shiftIdx(root, pos, 1);
    for (int i = reqCount; i > pos; i--) reqs[i] = reqs[i - 1];
    reqs[pos] = r; reqCount++;
    root = bstInsert(root, r.id, pos);
    return 1;
}
void removeRequest(int id) {
    BNode *n = bstSearch(root, id);
    if (!n) return;
    int idx = n->idx;
    root = bstDelete(root, id);
    for (int i = idx; i < reqCount - 1; i++) reqs[i] = reqs[i + 1];
    reqCount--;
    shiftIdx(root, idx + 1, -1);
}
int cmpReq(const Request *a, const Request *b, int key) {
    int d;
    cmpCount++;
    if (key == 1) d = a->priority - b->priority;
    else if (key == 2) { d = a->block - b->block; if (!d) d = a->room - b->room; }
    else d = strcmp(a->student, b->student);
    return d ? d : a->id - b->id;
}
void insertionSort(Request *a, int n, int key) {
    for (int i = 1; i < n; i++) {
        Request k = a[i];
        int j = i - 1;
        while (j >= 0 && cmpReq(&a[j], &k, key) > 0) { a[j + 1] = a[j]; j--; }
        a[j + 1] = k;
    }
}
void selectionSort(Request *a, int n, int key) {
    for (int i = 0; i < n - 1; i++) {
        int m = i;
        for (int j = i + 1; j < n; j++) if (cmpReq(&a[j], &a[m], key) < 0) m = j;
        if (m != i) { Request t = a[i]; a[i] = a[m]; a[m] = t; }
    }
}
void mergeSort(Request *a, Request *tmp, int l, int r, int key) {
    if (l >= r) return;
    int m = l + (r - l) / 2, i = l, j = m + 1, k = l;
    mergeSort(a, tmp, l, m, key);
    mergeSort(a, tmp, m + 1, r, key);
    while (i <= m && j <= r) tmp[k++] = (cmpReq(&a[i], &a[j], key) <= 0) ? a[i++] : a[j++];
    while (i <= m) tmp[k++] = a[i++];
    while (j <= r) tmp[k++] = a[j++];
    for (k = l; k <= r; k++) a[k] = tmp[k];
}
void runSort(int alg, Request *a, Request *tmp, int n, int key) {
    if (alg == 1) insertionSort(a, n, key);
    else if (alg == 2) selectionSort(a, n, key);
    else mergeSort(a, tmp, 0, n - 1, key);
}
int countStatusRec(int i, int status) { return i >= reqCount ? 0 : (reqs[i].status == status) + countStatusRec(i + 1, status); }
int prioritySumRec(int i) { return i >= reqCount ? 0 : (4 - reqs[i].priority) + prioritySumRec(i + 1); }   /* High=3 .. Low=1 */
int prioritySumIter(void) { int s = 0; for (int i = 0; i < reqCount; i++) s += 4 - reqs[i].priority; return s; }
void traverse(BNode *n, int mode) {                /* mode 0 = preorder, 1 = inorder, 2 = postorder */
    if (!n) return;
    if (mode == 0) printf("  %d (%s)\n", n->id, reqs[n->idx].student);
    traverse(n->left, mode);
    if (mode == 1) printf("  %d (%s)\n", n->id, reqs[n->idx].student);
    traverse(n->right, mode);
    if (mode == 2) printf("  %d (%s)\n", n->id, reqs[n->idx].student);
}
int countNodes(BNode *n) { return n ? 1 + countNodes(n->left) + countNodes(n->right) : 0; }
void printLeaves(BNode *n) {
    if (!n) return;
    if (!n->left && !n->right) printf("  %d (%s)\n", n->id, reqs[n->idx].student);
    printLeaves(n->left); printLeaves(n->right);
}
int openInBlock(int b) {
    int c = 0;
    for (int i = 0; b > 0 && i < reqCount; i++) if (reqs[i].block == 'A' + b - 1 && reqs[i].status != 2) c++;
    return c;
}
void bfs(int start) {
    int vis[BLOCKS] = {0}, dist[BLOCKS] = {0}, q[BLOCKS], f = 0, r = 0;
    vis[start] = 1; q[r++] = start;
    while (f < r) {
        int u = q[f++];
        printf("  %-8s hops: %d   open requests: %d\n", LOC[u], dist[u], openInBlock(u));
        for (int v = 0; v < BLOCKS; v++) if (adj[u][v] && !vis[v]) { vis[v] = 1; dist[v] = dist[u] + 1; q[r++] = v; }
    }
}
void dfs(int u, int vis[]) {
    vis[u] = 1;
    printf("%s(%d)", LOC[u], openInBlock(u));
    for (int v = 0; v < BLOCKS; v++) if (adj[u][v] && !vis[v]) { printf(" -> "); dfs(v, vis); }
}

/* ======================= shared actions ======================= */
void undoLast(void) {
    Action a;
    if (!pop(&a)) { printf("Nothing to undo.\n"); return; }
    BNode *n = bstSearch(root, a.req.id);
    if (a.type == ACT_ADD) {
        if (!n || reqs[n->idx].status != 0) { printf("Request #%d is already being handled; cannot undo.\n", a.req.id); return; }
        removeRequest(a.req.id);
        printf("Undo: request #%d withdrawn.\n", a.req.id);
    } else {
        if (n) { printf("Request #%d already exists.\n", a.req.id); return; }
        registerRequest(a.req);
        printf("Undo: request #%d restored.\n", a.req.id);
    }
}
void loadSample(void) {
    static const struct { const char *n; char b; int room, cat; const char *d; int p, st; } S[] = {
        {"Aarav Sharma", 'A', 101, 0, "Tap leaking in bathroom", 2, 0}, {"Riya Patel", 'C', 215, 1, "Fan not working", 2, 1},
        {"Karan Mehta", 'B', 312, 4, "WiFi router down on floor", 1, 0}, {"Sneha Iyer", 'E', 404, 2, "Broken study chair", 3, 0},
        {"Vikram Rao", 'D', 118, 1, "Short circuit near switchboard", 1, 0}, {"Isha Nair", 'A', 207, 3, "Corridor needs cleaning", 3, 2}
    };
    for (int i = 0; i < 6; i++) {
        Request r;
        memset(&r, 0, sizeof r);
        r.id = nextId++; r.block = S[i].b; r.room = S[i].room; r.priority = S[i].p; r.status = S[i].st;
        strcpy(r.student, S[i].n); strcpy(r.category, CATS[S[i].cat]); strcpy(r.desc, S[i].d);
        registerRequest(r);
    }
}

/* ======================= STUDENT SECTION ======================= */
int sameText(const char *a, const char *b) {
    while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) { a++; b++; }
    return *a == *b;
}
int isMine(const Request *r) { return sameText(r->student, myName) && r->block == myBlock && r->room == myRoom; }
int countMineRec(int i, int status) { return i >= reqCount ? 0 : (isMine(&reqs[i]) && reqs[i].status == status) + countMineRec(i + 1, status); }
int findMine(int id) {
    bstSteps = 0;
    BNode *n = bstSearch(root, id);
    if (!n || !isMine(&reqs[n->idx])) { printf("No request #%d found under your name.\n", id); return -1; }
    return n->idx;
}
void submitRequest(void) {
    Request r;
    memset(&r, 0, sizeof r);
    if (reqCount >= MAX_REQ) { printf("The request register is full. Contact the warden.\n"); return; }
    for (int i = 0; i < 6; i++) printf("  %d. %s\n", i + 1, CATS[i]);
    strcpy(r.category, CATS[readInt("Category: ", 1, 6) - 1]);
    do { readLine("Describe the problem: ", r.desc, DESC_LEN); } while (r.desc[0] == '\0');
    r.priority = readInt("Urgency (1=High: safety/no water/power, 2=Medium, 3=Low): ", 1, 3);
    r.id = nextId++; r.block = myBlock; r.room = myRoom; r.status = 0;
    strcpy(r.student, myName);
    registerRequest(r); rebuildQueue(); push(ACT_ADD, r);
    printf("Request #%d submitted. Queue position: %d of %d pending.\n", r.id, queuePos(r.id), cqCount);
}
void viewMine(void) {
    int c = 0;
    for (int i = 0; i < reqCount; i++)
        if (isMine(&reqs[i])) { if (!c++) printHeader(); printRow(&reqs[i]); }
    if (!c) printf("You have no requests yet.\n"); else printf("Total: %d request(s)\n", c);
}
void trackRequest(void) {
    int id = readInt("Request ID: ", 1, 999999), idx = findMine(id);
    if (idx < 0) return;
    printHeader(); printRow(&reqs[idx]);
    if (reqs[idx].status == 0) printf("Waiting in queue: %d request(s) ahead of you.\n", queuePos(id) - 1);
    else if (reqs[idx].status == 1) printf("A technician has been assigned and is working on it.\n");
    else printf("Your request has been resolved.\n");
}
void cancelRequest(void) {
    char ans[8];
    int id = readInt("Request ID to cancel: ", 1, 999999), idx = findMine(id);
    if (idx < 0) return;
    if (reqs[idx].status != 0) { printf("Only pending requests can be cancelled (this one is %s).\n", STATUS_NAME[reqs[idx].status]); return; }
    readLine("Are you sure? (y/n): ", ans, sizeof ans);
    if (tolower((unsigned char)ans[0]) != 'y') { printf("Nothing cancelled.\n"); return; }
    Request copy = reqs[idx];
    removeRequest(id); push(ACT_DELETE, copy);
    printf("Request #%d cancelled (use Undo to bring it back).\n", id);
}
void summary(void) {
    printf("\n--- Summary for %s (%c-%d) ---\n", myName, myBlock, myRoom);
    for (int s = 0; s < 3; s++) printf("%-9s : %d\n", STATUS_NAME[s], countMineRec(0, s));
}
void studentSession(void) {
    void (*act[])(void) = {NULL, submitRequest, viewMine, trackRequest, cancelRequest, undoLast, summary};
    int choice;
    printf("\n=== Student Login ===\n");
    do { readLine("Your full name: ", myName, NAME_LEN); } while (myName[0] == '\0');
    for (int i = 0; myName[i]; i++) myName[i] = (char)((i == 0 || myName[i - 1] == ' ') ? toupper((unsigned char)myName[i]) : tolower((unsigned char)myName[i]));
    myBlock = readBlock();
    myRoom = readInt("Room number (101-599): ", 101, 599);
    top = -1;
    do {
        printf("\n====== STUDENT MENU: %s (%c-%d) ======\n", myName, myBlock, myRoom);
        printf(" 1. Submit new maintenance request\n 2. View my requests\n 3. Track a request by ID\n");
        printf(" 4. Cancel a pending request\n 5. Undo last submit/cancel\n 6. My request summary\n 0. Logout\n");
        choice = readInt("Enter choice: ", 0, 6);
        rebuildQueue();
        if (choice) act[choice]();
    } while (choice != 0);
}

/* ======================= ADMIN SECTION ======================= */
void showAll(void) {
    if (!reqCount) { printf("No requests registered.\n"); return; }
    printHeader();
    for (int i = 0; i < reqCount; i++) printRow(&reqs[i]);
    printf("Total: %d request(s)\n", reqCount);
}
void showQueue(void) {
    printf("\nPending queue (most urgent first), %d waiting:\n", cqCount);
    for (int i = 0; i < cqCount; i++) {
        Request *r = &reqs[bstSearch(root, cq[(cqFront + i) % CQ_SIZE])->idx];
        printf("  %d. #%d %-14s %c-%d [%s] %s\n", i + 1, r->id, r->student, r->block, r->room, PRIO_NAME[r->priority], r->category);
    }
    if (!cqCount) printf("  (empty)\n");
}
void assignNext(void) {
    if (cqCount == 0) { printf("No pending requests in the queue.\n"); return; }
    Request *r = &reqs[bstSearch(root, cqDequeue())->idx];
    r->status = 1;
    printf("Technician assigned to #%d (%s, %c-%d: %s - %s)\n", r->id, r->student, r->block, r->room, r->category, r->desc);
}
void resolveRequest(void) {
    int id = readInt("Request ID to mark resolved: ", 1, 999999);
    BNode *n = bstSearch(root, id);
    if (!n) { printf("Request not found.\n"); return; }
    Request *r = &reqs[n->idx];
    if (r->status != 1) { printf("Only 'Assigned' requests can be resolved (current: %s).\n", STATUS_NAME[r->status]); return; }
    r->status = 2;
    printf("Request #%d marked resolved.\n", id);
}
void deleteRequest(void) {
    int id = readInt("Request ID to delete: ", 1, 999999);
    BNode *n = bstSearch(root, id);
    if (!n) { printf("Request not found.\n"); return; }
    Request copy = reqs[n->idx];
    removeRequest(id); push(ACT_DELETE, copy);
    printf("Request #%d deleted (use Undo to restore).\n", id);
}
void showLog(void) {
    typedef struct LNode { int id; char student[NAME_LEN], category[20]; struct LNode *next; } LNode;
    LNode *head = NULL, *tail = NULL, *n;
    for (int i = 0; i < reqCount; i++) {
        if (reqs[i].status != 2) continue;
        n = (LNode *)malloc(sizeof(LNode));
        n->id = reqs[i].id; n->next = NULL;
        strcpy(n->student, reqs[i].student); strcpy(n->category, reqs[i].category);
        if (!tail) head = tail = n; else { tail->next = n; tail = n; }
    }
    printf("\nResolved log (oldest first):\n");
    if (!head) printf("  (empty)\n");
    while (head) { n = head; head = head->next; printf("  #%d  %-14s %s\n", n->id, n->student, n->category); free(n); }
}
void searchById(void) {
    int id = readInt("Request ID: ", 1, 999999);
    int lo = 0, hi = reqCount - 1, steps = 0;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        steps++;
        if (reqs[mid].id == id) break;
        if (reqs[mid].id < id) lo = mid + 1; else hi = mid - 1;
    }
    bstSteps = 0;
    BNode *n = bstSearch(root, id);
    if (n) { printHeader(); printRow(&reqs[n->idx]); } else printf("Request not found.\n");
}
void linearSearchRoom(void) {
    char b = readBlock();
    int room = readInt("Room number: ", 101, 599), hits = 0;
    for (int i = 0; i < reqCount; i++)
        if (reqs[i].block == b && reqs[i].room == room) { if (!hits++) printHeader(); printRow(&reqs[i]); }
    if (!hits) printf("No requests for room %c-%d.\n", b, room);
}
void sortMenu(void) {
    static Request copy[MAX_REQ], tmp[MAX_REQ];
    static const char *names[] = {"", "Insertion", "Selection", "Merge"};
    static const char *cx[] = {"", "O(n^2) time, O(1) space", "O(n^2) time, O(1) space", "O(n log n) time, O(n) space"};
    if (reqCount < 2) { printf("Need at least two requests to sort.\n"); return; }
    printf("Sort by: 1.Priority  2.Block/Room  3.Student name\n");
    int key = readInt("Choice: ", 1, 3);
    memcpy(copy, reqs, sizeof(Request) * reqCount);
    runSort(3, copy, tmp, reqCount, key);
    printHeader();
    for (int i = 0; i < reqCount; i++) printRow(&copy[i]);
    printf("\nPerformance of the three sorts on n = %d (avg of %d runs)\n", reqCount, REPS);
    printf("%-10s %12s %12s   %s\n", "Algorithm", "Comparisons", "Time (us)", "Complexity");
    for (int alg = 1; alg <= 3; alg++) {
        memcpy(copy, reqs, sizeof(Request) * reqCount);
        cmpCount = 0;
        runSort(alg, copy, tmp, reqCount, key);
        long c = cmpCount;
        clock_t t = clock();
        for (int r = 0; r < REPS; r++) { memcpy(copy, reqs, sizeof(Request) * reqCount); runSort(alg, copy, tmp, reqCount, key); }
        printf("%-10s %12ld %12.2f   %s\n", names[alg], c, (double)(clock() - t) * 1e6 / CLOCKS_PER_SEC / REPS, cx[alg]);
    }
}
void bstMenu(void) {
    if (!root) { printf("Tree is empty.\n"); return; }
    printf("\nInorder (sorted by ID):\n"); traverse(root, 1);
    printf("Preorder:\n"); traverse(root, 0);
    printf("Postorder:\n"); traverse(root, 2);
    printf("Leaf nodes:\n"); printLeaves(root);
    printf("Total nodes: %d\n", countNodes(root));
}
void routePlanner(void) {
    int vis[BLOCKS] = {0};
    printf("\nBFS from Maintenance Office (nearest blocks first):\n");
    bfs(0);
    printf("\nDFS patrol route (open requests in brackets):\n  ");
    dfs(0, vis);
    printf("\n");
}
void statistics(void) {
    printf("\n--- Statistics ---\nTotal requests: %d\n", reqCount);
    for (int s = 0; s < 3; s++) printf("%-9s     : %d\n", STATUS_NAME[s], countStatusRec(0, s));
    printf("Urgency score (recursive / iterative): %d / %d\n", prioritySumRec(0), prioritySumIter());
    printf("BST nodes: %d, queue size: %d, undo depth: %d\n", countNodes(root), cqCount, top + 1);
}
void adminSession(void) {
    void (*act[])(void) = {NULL, showAll, showQueue, assignNext, resolveRequest, deleteRequest, undoLast,
                           showLog, searchById, linearSearchRoom, sortMenu, bstMenu, routePlanner, statistics};
    char pass[40];
    int choice;
    printf("\n=== Admin Login ===\n");
    readLine("Password: ", pass, sizeof pass);
    if (strcmp(pass, ADMIN_PASS) != 0) { printf("Wrong password. Access denied.\n"); return; }
    top = -1;
    do {
        printf("\n========== ADMIN MENU ==========\n");
        printf(" 1. View all requests          8. Search by ID\n 2. View pending queue         9. Search by room\n");
        printf(" 3. Assign next request       10. Sort + performance analysis\n 4. Mark request resolved     11. BST traversals & info\n");
        printf(" 5. Delete a request          12. Technician route\n 6. Undo last delete          13. Statistics\n");
        printf(" 7. Resolved log               0. Logout\n");
        choice = readInt("Enter choice: ", 0, 13);
        rebuildQueue();
        if (choice) act[choice]();
    } while (choice != 0);
}
int main(void) {
    int choice;
    loadSample();
    printf("Welcome to the Hostel Maintenance Request Manager! (sample data loaded)\n");
    do {
        printf("\n===== HOSTEL MAINTENANCE REQUEST MANAGER =====\n 1. Student login\n 2. Admin login\n 0. Exit\n");
        choice = readInt("Enter choice: ", 0, 2);
        if (choice == 1) studentSession();
        else if (choice == 2) adminSession();
    } while (choice != 0);
    freeTree(root);
    printf("Goodbye!\n");
    return 0;
}
