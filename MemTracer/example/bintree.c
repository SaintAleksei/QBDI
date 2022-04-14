#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

struct node
{
    struct node *left;
    struct node *right;
    int64_t data;
};

static struct node *treeAdd(struct node *root, int64_t data);
static void treePrint(const char *fname, struct node *root);
static void treePrintRecursive(FILE *outfile, struct node *root);

int main(int argc, char **argv)
{       
    if (argc == 1)
    {
        fprintf(stderr, "Usage: %s <data...>\n", argv[0]);

        return 1;
    }   

    struct node *root = NULL;
    for (int i = 1; i < argc; i++)
    {
        int64_t data;
        if (sscanf(argv[i], "%ld", &data) == 1)
            root = treeAdd(root, data);
    }

    char name[0x105] = "";
    strncpy(name, argv[0], 0x100);
    strncat(name, ".dot", 0x5);

    treePrint(name, root);

    return 0;
}

static struct node *treeAdd(struct node *node, int64_t data)
{
    if (!node)
    {
        node = malloc(sizeof(*node));
        assert(node);

        node->data = data;
        node->left = NULL;
        node->right = NULL;

        return node;
    }

    if (data > node->data)
        node->right = treeAdd(node->right, data);
    if (data < node->data)
        node->left = treeAdd(node->left, data);

    return node;
}

static void treePrint(const char *fname, struct node *root)
{
    assert(root);

    FILE *output = fopen(fname, "w");
    assert(output);
    
    fprintf(output, "Digraph G{\n");

    treePrintRecursive(output, root);

    fprintf(output, "}\n");
    
    fclose(output);
}

static void treePrintRecursive(FILE *outfile, struct node *root)
{
    assert(outfile);

    if (!root)
        return;
    
    fprintf(outfile, "\tnode0x%p[label=\"%p\"];\n", root, root);

    if (root->left)
        fprintf(outfile, "\tnode0x%p->node0x%p;\n", root, root->left);

    if (root->right)
        fprintf(outfile, "\tnode0x%p->node0x%p;\n", root, root->right);
    
    treePrintRecursive(outfile, root->left);
    treePrintRecursive(outfile, root->right);

}
