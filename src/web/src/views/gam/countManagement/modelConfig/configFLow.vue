<template>
  <div class="flow-container">
    <div class="flow-wrap">
      <VueFlow :nodes="nodes" :edges="edges" :node-types="nodeTypes" :default-zoom="0.4">
        <Background pattern-color="#e5e7eb" gap="16" />
        <Controls show-interactive />
      </VueFlow>
    </div>
  </div>
</template>

<script setup>
import { ref, watch, markRaw } from 'vue'
import { VueFlow, useVueFlow } from '@vue-flow/core'
import { Background } from '@vue-flow/background'
import { Controls } from '@vue-flow/controls'
import '@vue-flow/core/dist/style.css'
import '@vue-flow/controls/dist/style.css'
import { t } from '@/i18n'
import FlowBoxNode from './FlowBoxNode.vue'

const props = defineProps({
  flowData: {
    type: Object,
    default: () => ({})
  }
})
const nodes = ref([])

const edges = ref([])

const { onPaneReady } = useVueFlow()
onPaneReady((instance) => {
  instance.fitView()
})

const buildGraph = (data = {}) => {
  const inputs = Array.isArray(data.inputs) ? data.inputs : []
  const outputs = Array.isArray(data.outputs) ? data.outputs : []

  const inputRoot = {
    id: 'input-root',
    label: `${t('glossary.input')}\n(${inputs.length})`,
    position: { x: 100, y: 140 },
    type: 'box',
    data: {
      label: `${t('glossary.input')}\n(${inputs.length})`,
      kind: 'input',
      list: inputs,
    },
    style: {
      background: 'transparent',
      border: 'none',
      padding: 0
    }
  }

  const outputRoot = {
    id: 'output-root',
    label: `${t('glossary.output')}\n(${outputs.length})`,
    position: { x: 800, y: 140 },
    type: 'box',
    data: {
      label: `${t('glossary.output')}\n(${outputs.length})`,
      kind: 'output',
      list: outputs,
    },
    style: {
      background: 'transparent',
      border: 'none',
      padding: 0
    }
  }

  const mainEdge = {
    id: 'e-main',
    source: 'input-root',
    target: 'output-root',
    sourceHandle: 'right-out',
    targetHandle: 'left-in',
  }

  nodes.value = [inputRoot, outputRoot]
  edges.value = [mainEdge]
}

watch(
  () => props.flowData,
  (val) => {
    buildGraph(val)
  },
  { immediate: true, deep: true }
)

const nodeTypes = {
  box: markRaw(FlowBoxNode)
}
</script>

<style scoped lang="scss">
.flow-container {
  width: 100%;
}

.flow-wrap {
  width: 100%;
  height: 400px;
  border: 1px solid var(--el-border-color, #dcdfe6);
  border-radius: 6px;
  background: #fff;
}
</style>
