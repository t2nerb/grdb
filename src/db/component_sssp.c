#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <fcntl.h>
#include "graph.h"
#include "cli.h"


int count_vertices (component_t c)
{

    char* buf;
    int rdbytes;
    off_t offset;
    size_t size, len;

    size = (c->sv == NULL) ? 0 : schema_size(c->sv);

    rdbytes = sizeof(vertexid_t) + size;
    buf = malloc(rdbytes);

    int count = 0;
    for(;;) {
        lseek(c->vfd, offset, SEEK_SET);
        len = read(c->vfd, buf, rdbytes);
        if (len <= 0) break;

        offset += rdbytes;
        count++;
    }
    free(buf);
    return count;
}

attribute_t component_find_int_tuple(attribute_t a)
{
    attribute_t attr;

    for(attr = a; attr != NULL; attr = attr->next){
        if (attr->bt == 4) {
            return attr;
        }
    }

    return NULL;
}

void get_vertex_labels (component_t c, vertexid_t *list)
{
    int rdbytes;
    char* buf;
    ssize_t size, len;
    off_t offset;

    size = (c->sv == NULL) ? 0 : schema_size(c->sv);

    rdbytes = sizeof(vertexid_t) + size;
    buf = malloc(rdbytes);

    offset = 0;
    int index = 0;
    for (;;) {
        lseek(c->vfd, offset, SEEK_SET);
        len = read(c->vfd, buf, rdbytes);
        if (len <= 0) break;

        list[index] = *((vertexid_t *) buf);
        offset += rdbytes;
        index++;
    }

    free(buf);
}

int vertices_exist (vertexid_t *list, int nvertices,  vertexid_t v1, vertexid_t v2)
{
    int v1_exists = 0;
    int v2_exists = 0;

    for (int i = 0; i < nvertices; i++) {
        if (list[i] == v1) v1_exists++;
        else if (list[i] == v2) v2_exists++;
    }

    return (v1_exists && v2_exists) ? 1 : 0;

}

int get_index_by_id (vertexid_t *vertices, int nvertices, vertexid_t target)
{
    for (int i = 0; i < nvertices; i++) {
        if (vertices[i] == target) return i;
    }

    return -1;
}

int get_weight_from_edge(component_t c, vertexid_t v1, vertexid_t v2, char* attr_name)
{
    struct edge e;
    edge_t e1;
    int offset, weight;

    edge_init(&e);
    edge_set_vertices(&e, v1, v2);

    e1 = component_find_edge_by_ids(c, &e);
    if (e1 == NULL) {
        return INT_MAX;
    }

    if(e1 == NULL) return INT_MAX;

    offset = tuple_get_offset(e1->tuple, attr_name);
    weight = tuple_get_int(e1->tuple->buf + offset);

    return weight;
}


void init_dijkstra(int nvertices, int sindex, int *costs, vertexid_t *parents, int *visited)
{
    for (int i = 0; i < nvertices; i++) {

        visited[i] = 0;
        parents[i] = (vertexid_t) INT_MAX;

        if (i == sindex) {
            costs[i] = 0;
        }
        else {
            costs[i] = INT_MAX;
        }
    }
}


int get_min_index(vertexid_t *vertices, int nvertices, int *costs, vertexid_t *parents, int *visited)
{
    int mindex, min = INT_MAX;

    for (int i = 0; i < nvertices; i++) {
        if (visited[i] == 0 && costs[i] < min) {
            min = costs[i];
            mindex = i;
        }
    }

    visited[mindex]++;
    return mindex;
}

int not_all_visited(int *visited, int nvertices)
{
    for (int i = 0; i < nvertices; i++) {
        if (visited[i] == 1) return 1;
    }

    return 0;
}

void print_shortest_path(vertexid_t *vertices, int nvertices, vertexid_t *parents, vertexid_t v1, vertexid_t v2)
{
    vertexid_t sssp[nvertices];
    vertexid_t current_node = v2;

    int ctr = 0;
    while (current_node != INT_MAX) {
        sssp[ctr++] = current_node;
        int nindex = get_index_by_id(vertices, nvertices, current_node);
        current_node = parents[nindex];
    }

    printf(" Shortest Path: ");
    for (int i = ctr-1; i >= 0; i--) {
        if (i > 0) {
            printf(" %llu ->", sssp[i]);
        }
        else {
            printf(" %llu\n", sssp[i]);
        }
    }

}

int component_sssp (component_t c, vertexid_t v1, vertexid_t v2, int *n, int *total_weight, vertexid_t **path)
{
    int nvertices;
    int *visited, *costs, *distance;
    vertexid_t *vertices, *parents;
    attribute_t weight_attr;

    // File descriptors for vertices and edges
    c->efd = open("/root/.grdb/0/0/e", O_RDWR | O_CREAT, 0644);
    c->vfd = open("/root/.grdb/0/0/v", O_RDWR | O_CREAT, 0644);

    // Initialize variables to hold vertices and weights
    nvertices = count_vertices(c);
    vertices = malloc(nvertices * sizeof(vertexid_t));
    parents = malloc(nvertices * sizeof(vertexid_t));
    visited = malloc(nvertices * sizeof(int));
    costs = malloc(nvertices * sizeof(unsigned int));
    distance = malloc(nvertices * sizeof(int));

    // Figure out which attribute in the component edges schema you will
    // use for your weight function (I USE INTS OBV)
    weight_attr = component_find_int_tuple(c->se->attrlist);
    if (!weight_attr) {
        printf(" ERROR: Could not find attribute of type INT\n");
        return -1;
    }

    // Get all vertices and check if v1, v2 are in the vertex set
    get_vertex_labels(c, vertices);
    if (!vertices_exist(vertices, nvertices, v1, v2)) {
        printf(" ERROR: Source and destination must be in component\n");
        return -1;
    }

    //  Execute Dijkstra on the attribute you found for the specified
    //  component

    int has_out_path, sindex, findex;

    // Get index for start/end vertices and initialize costs and parents
    sindex = get_index_by_id(vertices, nvertices, v1);
    findex = get_index_by_id(vertices, nvertices, v2);
    init_dijkstra(nvertices, sindex, costs, parents, visited);

    // Update cost for each of the adjacent nodes
    has_out_path = 0;
    for (int i = 0; i < nvertices; i++) {
        int weight = get_weight_from_edge(c, vertices[sindex], vertices[i], weight_attr->name);
        if (weight != INT_MAX) {
            costs[i] = weight;
            parents[i] = vertices[sindex];
            has_out_path++;
        }
    }
    if (!has_out_path) {
        printf(" ERROR: %llu -> %llu does not exist!\n", v1, v2);
        return -1;
    }

    // Flag sindex was visited
    visited[sindex] = 1;

    while (not_all_visited(visited, nvertices)) {

        // Get next lowest cost node that wasn't visited and flag in visited
        // array
        sindex = get_min_index(vertices, nvertices, costs, parents, visited);
        distance[sindex] = costs[sindex];
        if (sindex == findex) break;

        // Update costs for adjacent nodes and relax do noods
        has_out_path = 0;
        for (int i = 0; i < nvertices; i++) {
            int weight = get_weight_from_edge(c, vertices[sindex], vertices[i], weight_attr->name);
            if (weight != INT_MAX) {
                int total_weight = distance[sindex] + weight;
                if (total_weight < costs[i] && visited[i] != 1) {
                    costs[i] = total_weight;
                    parents[i] = vertices[sindex];
                }
                has_out_path++;
            }

        }
        if (!has_out_path) {
            printf(" ERROR: %llu -> %llu does not exist!\n", v1, v2);
            return -1;
        }

    }

    print_shortest_path(vertices, nvertices, parents, v1, v2);
    printf(" Cost: %d\n", distance[sindex]);

    // Cleanup
    free(distance);
    free(vertices);
    free(visited);
    free(parents);
    free(costs);

    // I do what I want
    return 1;
}
