<template>
  <div
    class="stage-bracket"
    :style="{
      width: `${data.width}px`,
      height: `${data.height}px`
    }"
  >
    <div class="bracket-label" :style="{ color: data.color }">
      {{ data.titleKey ? t(data.titleKey) : data.title }}
    </div>
    <div class="bracket-line" :style="{ borderColor: data.stroke }"></div>
  </div>
</template>

<script setup>
import { t } from '@/i18n'

defineProps({
  data: {
    type: Object,
    default: () => ({})
  }
})
</script>

<style scoped>
.stage-bracket {
  box-sizing: border-box;
  pointer-events: none;
  position: relative;
}

.bracket-label {
  position: absolute;
  top: 0;
  left: 0;
  width: 100%;
  text-align: center;
  font-size: 12px;
  font-weight: 500;
  line-height: 18px;
  white-space: nowrap;
}

.bracket-line {
  position: absolute;
  top: 22px;
  left: 0;
  right: 0;
  height: 0;
  border-top: 1px solid;
  pointer-events: none;
}

.bracket-line::after {
  content: '';
  position: absolute;
  top: -1px;
  left: 50%;
  width: 22px;
  height: 2px;
  transform: translateX(-50%);
  background: var(--primary-color);
  border-radius: 1px;
}
</style>
