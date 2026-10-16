# study/papers/ —— 论文副本（本地保存，**不入库**）+ **怎么来的**

## 0. 一句话

PDF **不进仓库**（本人 2026-10-16 确认："原文就不用提交仓库了"），但**"将来怎么拿到一模一样的那份"进仓库** —— 就是本文件。
⇒ 换台电脑：`git clone` ⇒ 打开本文件 ⇒ 按 **§3** 的命令拉一份 ⇒ 用 **§3 的 SHA256** 核对，就能确认拿到的是**同一份**。

---

## 1. 为什么这个目录在仓库里只有 README

`.gitignore` 里有一条 `study/papers/*.pdf`：

```
# ---- study/ 下的论文副本（2026-10-16）：第三方版权材料 ⇒ 本地保存、不入库 ----
study/papers/*.pdf
```

⇒ **PDF 只留在本机**，不进仓库、不上传（仓库是 public，第三方版权材料不该被再分发）。
本 README 的作用就是：**克隆下来也能看见这个目录、并知道怎么把同一份拿回来**。

---

## 2. 这篇论文是什么

| 项 | 值 |
| --- | --- |
| 标题 | *A Fast Voxel Traversal Algorithm for Ray Tracing* |
| 作者 | John Amanatides · Andrew Woo（University of Toronto） |
| 出处 | **Eurographics 1987**（Proceedings, pp. 3–10） |
| DOI | `10.2312/egtp.19871000` |
| 为什么重要 | 我们 `engine/render/RayCaster.h` 的 DDA 格步进就是它（对照见 `study/RAYCASTING_NOTES.md` §2 / §4） |

---

## 3. 怎么来的（2026-10-16 实测）★ 换机器就按这张表复现

| # | 来源 | 拿到的方式 | 实测结果 | 文件指纹（SHA256） |
| --- | --- | --- | --- | --- |
| ① | **作者主页**（本机这份的来源，脚本可直下） | 见下面的 PowerShell | HTTP **200** · **17322 字节** · `PDF v1.2, 6 页` | ★ `05254dd9bed9a81d2c9da60e40bfe77b89e6b757428cc363a802a91ae835e5d5` |
| ② | **EG 官方 OA**（出版社自己的开放获取版，出处最"正规"） | 浏览器打开 `https://doi.org/10.2312/egtp.19871000` ⇒ 下载 PDF | ⚠️ 脚本直链（DSpace REST）返回 **401**（要鉴权）⇒ 只能**浏览器**下载；本机**没走这条** ⇒ 指纹未记录 | — |

```powershell
# ① 作者主页那份（原样可粘贴）
$dst = "$env:USERPROFILE\Downloads\Amanatides_Woo_1987_FastVoxelTraversal.pdf"
Invoke-WebRequest -Uri "http://www.cse.yorku.ca/~amana/research/grid.pdf" -OutFile $dst
(Get-FileHash $dst -Algorithm SHA256).Hash
# ★ 期望：05254DD9BED9A81D2C9DA60E40BFE77B89E6B757428CC363A802A91AE835E5D5
#   一致 ⇒ 就是同一份（本机 study/papers/ 下那份也是它：2026-10-16 重新下载核对过，逐字节相同）
```

**本机现有那份的指纹**：`17322 字节` · `PDF v1.2, 6 页` ·
`SHA256 = 05254dd9bed9a81d2c9da60e40bfe77b89e6b757428cc363a802a91ae835e5d5`

---

## 4. 关于"一样"的三条纪律（踩过一次坑，写下来）

| # | 纪律 | 为什么 |
| --- | --- | --- |
| 1 | **判"是不是同一份"只认 SHA256**，不认字节数 / 时间戳 | 2026-10-16 差点误判：这台机器的 `ls -la` / `stat` 在 `d:` 盘上报的大小**不可靠**（报 21418，真实 17322）⇒ 用 `sha256sum` 一核，两份**逐字节相同** |
| 2 | **不同来源的同一篇论文，哈希通常不同** | 重排版、加书签、加元数据都会改字节 ⇒ 所以这张表**按来源分别记指纹**，别拿 A 源的哈希去核 B 源的文件 |
| 3 | 来源失效时用 `https://web.archive.org/` 快照 | 记下"**检索日期 + 来源 URL + 哈希**"三件套，就永远能判断拿到的是不是同一份 |

---

## 5. 引用纪律（`AGENTS.md` §2 的目录约定）

- 文档里引用论文：只写 **出处 + 链接**，**不复制正文**；
- 想读全文：点 §3 的两个链接（公开可访问），或把 PDF 放进本目录（本机阅读）；
- ⚠️ **别把 PDF 提交进仓库**（`.gitignore` 已挡住；这也是本人 2026-10-16 的决定）。

> 相关：`study/RAYCASTING_NOTES.md` §7「权威阅读清单」（完整版，含每篇"为什么值得读"）。
