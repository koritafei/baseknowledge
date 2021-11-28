package biz

import (
	"hash/crc32"
	"sort"
	"strconv"
)

const MAXI = 1<<32 - 1

type KetamaHash struct {
	VirtualNodeNum uint32            // 虚拟节点数
	NodeHashMap    map[uint32]string // node hash
	AllSortedHv    []uint32          // index 数组
}

func NewKetamaHash() *KetamaHash {
	return &KetamaHash{
		VirtualNodeNum: 20,
		NodeHashMap:    make(map[uint32]string),
		AllSortedHv:    make([]uint32, 0),
	}
}

func (k *KetamaHash) hash_value(node string) uint32 {
	return crc32.ChecksumIEEE([]byte(node))
}

func (k *KetamaHash) ketama_virtual_node_array(v uint32) []uint32 {
	step := MAXI / k.VirtualNodeNum
	virtual_node := make([]uint32, k.VirtualNodeNum)
	var i uint32

	for i = 0; i < k.VirtualNodeNum; i++ {
		// 初始化虚拟节点列表
		virtual_node[i] = k.hash_value(strconv.FormatUint(uint64(v)+uint64(step)*uint64(i), 10))
	}

	return virtual_node
}

func (k *KetamaHash) sorted_map() {

	for hashv, _ := range k.NodeHashMap {
		k.AllSortedHv = append(k.AllSortedHv, hashv)
	}

	sort.Slice(k.AllSortedHv, func(i, j int) bool {
		return k.AllSortedHv[i] < k.AllSortedHv[j]
	})

}

func (k *KetamaHash) binary_search_node(node_hash_map map[uint32]string, all_sorted_hv []uint32, key string) string {
	hashv := k.hash_value(key)
	i := sort.Search(len(all_sorted_hv), func(i int) bool {
		return all_sorted_hv[i] >= hashv
	})

	if i >= len(all_sorted_hv) {
		i = 0
	}

	return node_hash_map[all_sorted_hv[i]]
}

func (k *KetamaHash) ketama_node_map_init(nodes []string, virutal_node uint32) {
	for _, s := range nodes {
		x := k.hash_value(s)
		k.VirtualNodeNum = virutal_node
		hash_arr := k.ketama_virtual_node_array(x)
		for _, iv := range hash_arr {
			k.NodeHashMap[iv] = s
		}
	}

	k.sorted_map()

	return
}

func (k *KetamaHash) GetNodeKetama(key string) string {
	node := k.binary_search_node(k.NodeHashMap, k.AllSortedHv, key)
	return node
}
