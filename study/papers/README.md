# study/papers/ —— 论文副本（本地保存，**不入库**）

## 为什么这个目录在仓库里是"空的"

`.gitignore` 里有一条 `study/papers/*.pdf`：

```
# ---- study/ 下的论文副本（2026-10-16）：第三方版权材料 ⇒ 本地保存、不入库 ----
study/papers/*.pdf
```

⇒ **PDF 只留在本机**，不进仓库、不上传（仓库是 public，第三方版权材料不该被再分发）。
本 README 只是为了"克隆下来也能看见这个目录存在、知道该往里放什么"。

## 里面应该有什么（怎么再拿一份）

| 文件 | 出处（怎么拿到） |
| --- | --- |
| `Amanatides_Woo_1987_FastVoxelTraversal.pdf` | Amanatides & Woo, *A Fast Voxel Traversal Algorithm for Ray Tracing*, Eurographics 1987 —— 我们 DDA 的原始论文。**官方 OA 版**：`https://doi.org/10.2312/egtp.19871000`（EG 数字图书馆）· **作者主页**：`http://www.cse.yorku.ca/~amana/research/grid.pdf`（2026-10-16 实测 HTTP 200、6 页 PDF） |

## 引用纪律（`AGENTS.md` §2 的目录约定）

- 文档里引用论文：只写 **出处 + 链接**，**不复制正文**；
- 想读全文：点上面两个链接（公开可访问），或把 PDF 放进本目录（本机阅读）。

> 相关：`study/RAYCASTING_NOTES.md` §7「权威阅读清单」（这张表的完整版，含每篇"为什么值得读"）。
