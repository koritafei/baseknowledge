package hash_alogarithm

import (
	"errors"
	"fmt"
	"hash/crc32"
	"sort"
)

// 一致性hash实现

type Consistent struct {
	nodesReplicas int               // 虚拟节点数
	hashSortNodes []uint32          // 对所有节点排序的数组
	circle        map[uint32]string // 所有节点对应的node
	nodes         map[string]bool   // node是否存在
}

// init
func NewConsistent() *Consistent {
	return &Consistent{
		nodesReplicas: 20,
		circle:        make(map[uint32]string),
		nodes:         make(map[string]bool),
	}
}

// add node
func (c *Consistent) Add(node string) error {
	if _, ok := c.nodes[node]; ok {
		// exists
		return fmt.Errorf("%s already exists", node)
	}

	c.nodes[node] = true
	for i := 0; i < c.nodesReplicas; i++ {
		// virtual nodes
		replicasKey := getReplicaKey(i, node)
		c.circle[replicasKey] = node
		c.hashSortNodes = append(c.hashSortNodes, replicasKey) // 所有节点ID
	}

	sort.Slice(c.hashSortNodes, func(i, j int) bool {
		return c.hashSortNodes[i] < c.hashSortNodes[j]
	})

	return nil
}

// remove node
func (c *Consistent) Remove(node string) error {
	if _, ok := c.nodes[node]; ok {
		return fmt.Errorf("%s not exists", node)
	}

	delete(c.nodes, node)
	for i := 0; i < c.nodesReplicas; i++ {
		replicasKey := getReplicaKey(i, node)
		delete(c.circle, replicasKey)
	}

	c.refreshHashSortNode()
	return nil
}

func (c *Consistent) GetNode() (node []string) {
	for v := range c.nodes {
		node = append(node, v)
	}

	return node
}

func (c *Consistent) Get(key string) (string, error) {
	if len(c.nodes) == 0 {
		return "", errors.New("not add node")
	}

	index := c.searchByIndex(key)
	host := c.circle[c.hashSortNodes[index]]

	return host, nil
}

func (c *Consistent) refreshHashSortNode() {
	c.hashSortNodes = nil

	for v := range c.circle {
		c.hashSortNodes = append(c.hashSortNodes, v)
	}

	sort.Slice(c.hashSortNodes, func(i, j int) bool {
		return c.hashSortNodes[i] < c.hashSortNodes[j]
	})
}

func (c *Consistent) searchByIndex(key string) int {
	hashkey := hashKey(key)
	index := sort.Search(len(c.hashSortNodes), func(i int) bool {
		return c.hashSortNodes[i] >= hashkey
	})

	if index >= len(c.hashSortNodes) {
		index %= len(c.hashSortNodes)
	}

	return index
}

func getReplicaKey(i int, node string) uint32 {
	return hashKey(fmt.Sprintf("%s#%d", node, i))
}

func hashKey(key string) uint32 {
	return crc32.ChecksumIEEE([]byte(key))
}
