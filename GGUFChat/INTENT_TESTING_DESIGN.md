# 意图识别测试方案设计

## 一、测试目标

### 1.1 核心指标
- **准确率 (Accuracy)**：模型正确识别意图的比例
- **召回率 (Recall)**：实际有意图的查询中，被正确识别的比例
- **精确率 (Precision)**：识别为某意图的查询中，真正属于该意图的比例
- **F1 Score**：精确率和召回率的调和平均
- **置信度分布**：了解模型的置信度水平

### 1.2 特殊关注点
- **chat-chat 回退准确率**：chat-chat 意图是否正确回退到 LLM
- **阈值敏感性**：不同阈值下的性能变化
- **边界情况处理**：短文本、长文本、多意图混合、无意义输入

## 二、测试数据准备方案

### 方案 1：人工标注测试集（推荐用于正式评估）

#### 优点
- 数据质量高
- 可控制数据分布
- 适合评估特定场景

#### 数据收集方式

**A. 头脑风暴法**
```
步骤：
1. 列出所有支持的意图（如 weather, music, chat）
2. 每个意图准备 20-30 个变体
3. 包含不同表达方式：
   - 完整句："今天北京天气怎么样"
   - 简短句："查天气"
   - 口语化："北京今儿天气咋样"
   - 带噪音："嗯...今天天气怎么样呢"
```

**B. 众包标注**
```
使用平台：
- Amazon MTurk
- 国内众包平台（如数据堂、京东众智）

任务设计：
1. 给标注员一个意图类别
2. 让他们写出 10 种不同的表达方式
3. 质量控制：多人标注，投票决定
```

**C. 真实日志采样**
```
从现有系统日志中：
1. 随机抽样用户查询
2. 人工标注正确意图
3. 注意隐私脱敏
```

#### 数据格式建议

**CSV 格式**
```csv
text,intent,confidence_threshold,expected_result,category,difficulty
今天北京天气怎么样,weather-weather,0.85,HIT,normal,easy
天气,weather-weather,0.85,NO_HIT,boundary,hard
播放周杰伦的歌,music-play,0.85,HIT,normal,easy
你好,chat-chat,0.85,HIT_FALLBACK,chat,easy
asdfgh,UNKNOWN,0.85,NO_HIT,noise,hard
```

**JSON 格式**
```json
{
  "test_cases": [
    {
      "id": "001",
      "text": "今天北京天气怎么样",
      "expected_intent": "weather-weather",
      "expected_result": "HIT",
      "slots": {
        "city": "北京",
        "date": "今天"
      },
      "category": "normal",
      "difficulty": "easy"
    }
  ]
}
```

### 方案 2：合成数据集（快速开始）

#### 模板生成法
```python
templates = {
    "weather": [
        "{date}{city}天气怎么样",
        "查询{city}{date}的天气",
        "{city}{date}会下雨吗"
    ]
}

cities = ["北京", "上海", "深圳", "广州"]
dates = ["今天", "明天", "后天", "周末"]

# 组合生成
for template in templates["weather"]:
    for city in cities:
        for date in dates:
            test_case = template.format(city=city, date=date)
```

#### 优缺点
- ✅ 快速生成大量数据
- ✅ 覆盖各种组合
- ❌ 缺乏真实性
- ❌ 可能过拟合模板

### 方案 3：对抗性样本（高级）

专门设计容易混淆的测试用例：

```
容易混淆的例子：
1. "今天天气真好" vs "今天天气怎么样"
   - 前者是陈述（chat），后者是查询（weather）

2. "播放天气预报" vs "播放音乐"
   - 同样是播放，但意图不同

3. "北京" vs "去北京" vs "北京天气"
   - 不同完整度的查询
```

## 三、测试执行方案

### 方案 1：手动测试（适合初期探索）

#### 工具：使用应用的聊天界面

**步骤**：
1. 准备测试用例列表（Excel/Notion）
2. 逐条输入到聊天框
3. 观察结果并记录：
   - 实际识别的意图
   - 置信度
   - 是否符合预期
4. 统计准确率

**优点**：
- 直观
- 可以同时测试 UI 体验

**缺点**：
- 耗时
- 容易出错
- 难以重现

### 方案 2：脚本测试（推荐）

#### 选项 A：Python 脚本（离线）

如果你能导出模型：

```python
import onnxruntime as ort
import pandas as pd

# 加载测试数据
df = pd.read_csv('test_cases.csv')

# 加载模型
session = ort.InferenceSession('model.onnx')

results = []
for _, row in df.iterrows():
    text = row['text']
    expected = row['intent']

    # 推理
    output = session.run(None, {input: preprocess(text)})
    predicted = postprocess(output)

    results.append({
        'text': text,
        'expected': expected,
        'predicted': predicted,
        'correct': expected == predicted
    })

# 统计
accuracy = sum(r['correct'] for r in results) / len(results)
print(f"Accuracy: {accuracy:.2%}")
```

#### 选项 B：Android Instrumentation Test

在应用内编写自动化测试：

```kotlin
@RunWith(AndroidJUnit4::class)
class IntentRecognitionTest {

    @Test
    fun testIntentAccuracy() {
        val testCases = loadTestCases("test_data.csv")

        var correct = 0
        val results = mutableListOf<TestResult>()

        testCases.forEach { case ->
            val result = IntentRecognizerManager.predict(case.text)
            val isCorrect = result?.intent == case.expectedIntent

            if (isCorrect) correct++
            results.add(TestResult(case, result, isCorrect))
        }

        val accuracy = correct.toFloat() / testCases.size

        // 生成报告
        generateReport(results, accuracy)

        // 断言
        assertTrue("Accuracy should be >= 85%", accuracy >= 0.85f)
    }
}
```

#### 选项 C：通过 logcat 批量测试

```bash
# 准备测试脚本
cat test_cases.txt | while read line; do
    # 通过 adb 触发测试
    adb shell "am broadcast -a com.stdemo.ggufchat.TEST_INTENT --es text '$line'"

    # 读取 logcat 结果
    adb logcat -d | grep "IntentResult"
    sleep 0.5
done
```

### 方案 3：持续监控（生产环境）

在实际使用中收集数据：

```kotlin
// 记录所有预测
fun predict(text: String): IntentResult? {
    val result = recognizer.predict(text)

    // 记录到本地或上传到服务器
    logPrediction(
        text = text,
        intent = result?.intent,
        confidence = result?.confidence,
        hit = result?.hit,
        timestamp = System.currentTimeMillis()
    )

    return result
}

// 定期分析
fun analyzeLogs() {
    val logs = loadPredictionLogs()

    // 统计
    val avgConfidence = logs.map { it.confidence }.average()
    val hitRate = logs.count { it.hit }.toFloat() / logs.size

    // 找出低置信度案例
    val lowConfidence = logs.filter { it.confidence < 0.9 }

    // 上报
    sendAnalytics(avgConfidence, hitRate, lowConfidence)
}
```

## 四、评估指标详解

### 4.1 基础指标

#### 准确率 (Accuracy)
```
Accuracy = 正确预测数 / 总预测数
```

**适用场景**：各类意图分布均衡时

**局限**：当某个意图占比很大时会被误导

#### 示例
```
100 个测试用例：
- 90 个 chat-chat（全部正确）
- 10 个 weather（全部错误）

Accuracy = 90/100 = 90%  ← 看起来很好，但 weather 全错了！
```

### 4.2 分类指标（每个意图单独计算）

#### 精确率 (Precision)
```
Precision = 真正例 / (真正例 + 假正例)
```

**含义**：识别为某意图的查询中，真正属于该意图的比例

#### 召回率 (Recall)
```
Recall = 真正例 / (真正例 + 假反例)
```

**含义**：实际属于某意图的查询中，被正确识别的比例

#### F1 Score
```
F1 = 2 × (Precision × Recall) / (Precision + Recall)
```

**含义**：精确率和召回率的调和平均

#### 混淆矩阵示例

```
              预测
         weather  music  chat  NO_HIT
实际 weather   15      1     2      2
     music      0     18     1      1
     chat       1      0    48      1
     NO_HIT     2      1     3      4
```

从混淆矩阵可以看出：
- weather → chat: 2 次误判
- NO_HIT → weather: 2 次误判
- ...

### 4.3 阈值相关指标

#### ROC 曲线
在不同阈值下，画出 TPR vs FPR 曲线

```python
thresholds = [0.5, 0.6, 0.7, 0.8, 0.85, 0.9, 0.95]
for threshold in thresholds:
    results = test_with_threshold(threshold)
    plot_point(results.tpr, results.fpr)
```

#### 最优阈值选择
```
目标：找到平衡点
- 阈值太低 → 很多误判（假正例多）
- 阈值太高 → 漏掉真实意图（假反例多）

方法：
1. F1 Score 最大化
2. 业务指标优化（如最小化用户投诉）
```

### 4.4 特定指标

#### chat-chat 回退率
```
chat-chat 回退率 = (正确回退的 chat / 所有 chat) × 100%

目标：≥ 95%（大部分 chat 应该回退到 LLM）
```

#### 意图覆盖率
```
意图覆盖率 = (有测试用例的意图数 / 总意图数) × 100%

目标：100%（每个意图都要测试）
```

## 五、测试流程建议

### 阶段 1：基线测试（第一次）

**目标**：了解当前性能

```
步骤：
1. 准备 50-100 个测试用例
   - 每个意图至少 10 个
   - 包含正常和边界情况

2. 使用当前阈值（0.85）测试

3. 记录结果：
   - 总体准确率
   - 每个意图的 P/R/F1
   - 失败案例列表

4. 分析：
   - 哪些意图表现好？
   - 哪些意图表现差？
   - 常见错误模式是什么？
```

### 阶段 2：阈值调优

**目标**：找到最优阈值

```
步骤：
1. 测试不同阈值：[0.6, 0.7, 0.75, 0.8, 0.85, 0.9, 0.95]

2. 对比指标：
   - Accuracy
   - F1 Score
   - 用户体验（需要人工评估）

3. 选择最优阈值（通常在 0.75-0.90 之间）
```

### 阶段 3：错误分析与迭代

**目标**：针对性改进

```
步骤：
1. 分析所有失败案例

2. 分类错误类型：
   - 数据标注错误（修正标签）
   - 模型混淆（需要更多训练数据）
   - 阈值问题（调整阈值）
   - 边界情况（考虑特殊处理）

3. 针对性改进：
   - 补充训练数据
   - 调整模型参数
   - 添加规则补充

4. 重新测试
```

### 阶段 4：真实环境验证

**目标**：在真实场景中验证

```
步骤：
1. 小范围灰度发布（5-10% 用户）

2. 收集真实数据：
   - 用户满意度
   - 意图识别准确率
   - 低置信度查询

3. 对比测试集和真实数据的差异

4. 补充测试集（用真实数据）

5. 全量发布
```

## 六、工具推荐

### 6.1 数据管理
- **Excel/Google Sheets**：简单的测试用例管理
- **Notion/Airtable**：更强大的数据库功能
- **Git + CSV**：版本控制测试数据

### 6.2 标注工具
- **Label Studio**：开源标注平台
- **doccano**：文本标注工具
- **自建表单**：Google Forms + Scripts

### 6.3 分析工具
- **Python + Pandas**：数据分析
- **Jupyter Notebook**：交互式分析
- **Matplotlib/Seaborn**：可视化

### 6.4 持续集成
- **GitHub Actions**：自动运行测试
- **Firebase Test Lab**：Android 自动化测试
- **自建 CI/CD**：Jenkins + Android Emulator

## 七、推荐实施路线

### 最小方案（快速验证）
```
时间：1-2 天
成本：低

步骤：
1. 手动准备 30 个测试用例（每个意图 10 个）
2. 使用应用界面手动测试
3. 记录结果到 Excel
4. 计算准确率
5. 调整阈值

输出：
- 准确率数字
- 明显的问题列表
```

### 标准方案（正式评估）
```
时间：1 周
成本：中等

步骤：
1. 准备 100-200 个测试用例
   - 使用 CSV 格式
   - 包含各种场景

2. 编写简单的测试脚本（Python 或 Kotlin）
   - 批量测试
   - 自动计算指标

3. 生成测试报告
   - 准确率、P/R/F1
   - 混淆矩阵
   - 失败案例分析

4. 迭代优化

输出：
- 详细测试报告
- 优化建议
- 最优阈值
```

### 完整方案（生产级）
```
时间：2-4 周
成本：高

步骤：
1. 建立完整测试集（500+ 用例）
   - 众包标注
   - 真实数据采样

2. 自动化测试框架
   - CI/CD 集成
   - 每次变更自动测试

3. 持续监控
   - 生产环境数据收集
   - 定期分析报告

4. A/B 测试
   - 对比不同阈值/模型版本
   - 数据驱动决策

输出：
- 完整测试体系
- 持续优化机制
- 性能仪表盘
```

## 八、注意事项

### 8.1 数据质量
- ✅ 标注要准确一致
- ✅ 覆盖真实场景
- ✅ 包含边界情况
- ❌ 避免数据泄露（测试集出现在训练集中）

### 8.2 评估偏差
- ⚠️ 测试集不能代表所有用户
- ⚠️ 人工标注可能有主观性
- ⚠️ 指标要结合业务目标

### 8.3 持续迭代
- 📈 测试不是一次性的
- 📈 随着用户增长，持续收集数据
- 📈 定期重新评估

## 总结

选择合适的测试方案取决于：
1. **项目阶段**：原型 vs 生产
2. **资源**：时间、人力、预算
3. **要求**：快速验证 vs 严格评估

**建议起步**：
- 先用最小方案快速验证（30 个用例，手动测试）
- 如果效果可以，再扩展到标准方案
- 生产环境后，建立持续监控机制
