#include "rstack.h"
#include "stdlib.h"
#include "stdio.h"

typedef enum
{
    VALUE,
    RSTACK,
    SENTINEL
} elem_type;

struct rstack
{
    elem_type type;
    union
    {
        uint64_t val;
        struct rstack *stack;
    };
    struct rstack *next;
    struct rstack *before;
    struct rstack *prevDeclared;
    bool access;
    bool visited;
};

static rstack_t *__PREVIOUS = nullptr;

rstack_t *rstack_new()
{
    rstack_t *result = (rstack_t *)malloc(sizeof(struct rstack));
    if (result != nullptr)
    {
        result->type = SENTINEL;
        result->access = 1;
        result->visited = 0;
        result->next = result;
        result->prevDeclared = __PREVIOUS;
        result->before = result;
    }
    __PREVIOUS = result;

    return result;
}

int rstack_push_value(rstack_t *rs, uint64_t value)
{
    if (rs == nullptr)
    {
        errno = EINVAL;
        return -1;
    }
    rstack_t *elem = (rstack_t *)malloc(sizeof(struct rstack));
    if (elem == nullptr)
    {
        errno = ENOMEM;
        return -1;
    }
    elem->type = VALUE;
    elem->val = value;
    elem->next = rs->next;
    elem->before = rs;
    elem->next->before = elem;
    elem->visited = 0;
    rs->next = elem;

    return 0;
}

int rstack_push_rstack(rstack_t *rs1, rstack_t *rs2)
{
    if (rs1 == nullptr || rs2 == nullptr)
    {
        errno = EINVAL;
        return -1;
    }
    rstack_t *elem = (rstack_t *)malloc(sizeof(struct rstack));
    if (elem == nullptr)
    {
        errno = ENOMEM;
        return -1;
    }
    elem->type = RSTACK;
    elem->next = rs1->next;
    elem->before = rs1;
    elem->visited = 0;

    rs1->next->before = elem;
    rs1->next = elem;
    elem->stack = rs2;

    return 0;
}

result_t bfs(rstack_t *rs)
{
    result_t default_result = {false, 0};
    if (rs == nullptr || rs->visited == 1)
        return default_result;

    rs->visited = 1;
    rstack_t *elem = rs->next;
    while (elem->type != SENTINEL)
    {
        if (elem->type == VALUE)
        {
            result_t result = {true, elem->val};
            return result;
        }
        else if (elem->type == RSTACK)
        {
            result_t result = bfs(elem->stack);
            if (result.flag == true)
                return result;
        }
        elem = elem->next;
    }
    return default_result;
}

void clearing(rstack_t *rs)
{
    if (rs == nullptr || rs->visited == 0)
        return;

    rs->visited = 0;
    rstack_t *child = rs->next;
    while (child->type != SENTINEL)
    {
        if (child->type == RSTACK)
            clearing(child->stack);

        child = child->next;
    }
    return;
}

result_t rstack_front(rstack_t *rs)
{
    result_t final_result = bfs(rs);
    clearing(rs);
    return final_result;
}

bool rstack_empty(rstack_t *rs)
{
    if (rstack_front(rs).flag == true)
        return false;

    return true;
}

int rstack_write(char const *path, rstack_t *rs)
{
    FILE *f = fopen(path, "w");
    if (f == nullptr)
        return -1;
    int cycle_detected = 0;
    int res = writing(rs, &cycle_detected, f);
    if (ferror(f))
        res = -1;
    fclose(f);
    return res;
}

int writing(rstack_t *rs, int *cycle_detected, FILE *f)
{

    if (rs == nullptr || f == nullptr)
        return -1;

    if (rs->visited)
    {
        if (cycle_detected)
            *cycle_detected = 1;
        return 0;
    }
    rs->visited = 1;

    rstack_t *elem = rs->before;

    while (elem->type != SENTINEL)
    {
        if (elem->type == VALUE)
        {
            if (fprintf(f, "%" PRIu64 "\n", elem->val) < 0)
            {
                rs->visited = 0;
                return -1;
            }
        }

        else if (elem->type == RSTACK)
        {
            int res = writing(elem->stack, cycle_detected, f);
            if (res == -1 || *cycle_detected == 1)
            {
                rs->visited = 0;
                return res;
            }
        }
        elem = elem->before;
    }
    rs->visited = 0;
    return 0;
}

void dfs_deleter(rstack_t *rs)
{
    if (rs == nullptr)
        return;

    rstack_t *actual = rs;
    actual->visited = 1;

    actual = actual->next;
    while (actual->type != SENTINEL)
    {
        if (actual->type == RSTACK && actual->stack != nullptr && actual->stack->visited == 0)
            dfs_deleter(actual->stack);
        actual = actual->next;
    }
}

void rstack_delete(rstack_t *rs)
{
    if (rs == nullptr)
        return;

    rs->access = 0;
    rstack_t *listIndex = __PREVIOUS;
    while (listIndex != nullptr)
    {
        if (listIndex->access == 1)
            dfs_deleter(listIndex);
        listIndex = listIndex->prevDeclared;
    }
    listIndex = __PREVIOUS;
    while (listIndex != nullptr && listIndex->visited == 0)
    {
        rstack_t *temp = listIndex; // poczatek listy
        listIndex = listIndex->prevDeclared;
        __PREVIOUS = listIndex;
        free(temp);
    }

    while (listIndex != nullptr)
    {
        if (listIndex->prevDeclared != nullptr && listIndex->prevDeclared->visited == 0)
        {
            rstack_t *to_delete = listIndex->prevDeclared;
            listIndex->prevDeclared = to_delete->prevDeclared;
            free(to_delete);
        }
        else
            listIndex = listIndex->prevDeclared;
    }
    rstack_t *cleanup = __PREVIOUS;
    while (cleanup != nullptr)
    {
        cleanup->visited = 0;
        cleanup = cleanup->prevDeclared;
    }
}

void rstack_pop(rstack_t *rs)
{
    if (rs == nullptr || rs->next->type == SENTINEL)
        return;

    rstack_t *temp = rs->next;
    rs->next->next->before = rs;
    rs->next = rs->next->next;

    if (temp->type == RSTACK && temp->stack->access == 0)
        rstack_delete(temp->stack);

    free(temp);

    return;
}
rstack_t* rstack_read(const char *path)
{
    if (path == nullptr)
    {
        errno = EINVAL;
        return nullptr;
    }

    FILE *f = fopen(path, "r");
    if (f == nullptr)
        return nullptr;

    rstack_t *rs = rstack_new();
    if (rs == nullptr)
    {
        fclose(f);
        errno = ENOMEM;
        return nullptr;
    }

    uint64_t val;

    while (fscanf(f, "%" SCNu64, &val) == 1)
    {
        if (rstack_push_value(rs, val) != 0)
        {
            rstack_delete(rs);
            fclose(f);
            return nullptr;
        }
    }

    // jeśli nie EOF → błąd w pliku
    if (!feof(f))
    {
        rstack_delete(rs);
        fclose(f);
        errno = EINVAL;
        return nullptr;
    }

    fclose(f);
    return rs;
}