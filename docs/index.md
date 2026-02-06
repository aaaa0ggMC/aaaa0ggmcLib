# aaaa0ggmc's library gen5 文档

## 目录
- [aaaa0ggmc's library gen5 文档](#aaaa0ggmcs-library-gen5-文档)
  - [目录](#目录)
  - [前言](#前言)
  - [组件](#组件)

## 前言
说实话,这个项目到目前做了快4年还是让我十分震惊的,基本上是随着我的MineTerraria->UnlimitedLife的初心不变的探索而成长,不过这么久了我一直还没有写过这个项目对应的文档.
因此这次把库的核心之一adata写完后我便准备先把adata的documents写完.
2026/2/6

## 组件
- [adata](./adata/index.md)| aaaa0ggmcLib的通用数据处理,聚焦于动态的配置文件,提供了类似nlohmann/json的体验(肯定还是比不上大佬的库)以及自制的校验(准确来说是Sanitize)方案来保证对于配置文件不需要担心数据类型问题.