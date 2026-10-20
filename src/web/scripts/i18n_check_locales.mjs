import fs from 'node:fs'
import path from 'node:path'
import { fileURLToPath, pathToFileURL } from 'node:url'

const __dirname = path.dirname(fileURLToPath(import.meta.url))
const srcDir = path.resolve(__dirname, '..', 'src')
const localeDir = path.join(srcDir, 'i18n', 'locales')

const flattenKeys = (value, prefix = '') => {
  if (!value || typeof value !== 'object' || Array.isArray(value)) return [prefix]
  return Object.entries(value).flatMap(([key, child]) => flattenKeys(child, prefix ? `${prefix}.${key}` : key))
}

const importLocale = async (name) => {
  const moduleUrl = pathToFileURL(path.join(localeDir, `${name}.js`)).href
  const module = await import(moduleUrl)
  return module.default
}

const [zhCN, enUS] = await Promise.all([
  importLocale('zh-CN'),
  importLocale('en-US')
])

const zhKeys = new Set(flattenKeys(zhCN))
const enKeys = new Set(flattenKeys(enUS))
const missingInZh = [...enKeys].filter(key => !zhKeys.has(key))
const missingInEn = [...zhKeys].filter(key => !enKeys.has(key))
const shortKeys = [...enKeys].filter(key => key.startsWith('short.'))
const orphanShortKeys = shortKeys
  .map(key => key.slice('short.'.length))
  .filter(key => !enKeys.has(key))

if (missingInZh.length || missingInEn.length || orphanShortKeys.length) {
  if (missingInZh.length) {
    console.error('Missing zh-CN locale keys:')
    missingInZh.forEach(key => console.error(`- ${key}`))
  }
  if (missingInEn.length) {
    console.error('Missing en-US locale keys:')
    missingInEn.forEach(key => console.error(`- ${key}`))
  }
  if (orphanShortKeys.length) {
    console.error('Short locale keys without full keys:')
    orphanShortKeys.forEach(key => console.error(`- short.${key} -> ${key}`))
  }
  process.exit(1)
}

const EXTENSIONS = new Set(['.vue', '.js', '.mjs', '.ts'])
const collectFiles = (dir) => fs.readdirSync(dir, { withFileTypes: true }).flatMap((entry) => {
  const full = path.join(dir, entry.name)
  if (entry.isDirectory()) {
    if (entry.name === 'node_modules' || entry.name === 'dist') return []
    return collectFiles(full)
  }
  return EXTENSIONS.has(path.extname(entry.name)) ? [full] : []
})

// Statically referenced translation calls and resource metadata keys.
const keyPatterns = [
  /(?:^|[^\w$])(?:t|\$t|proxy\.\$t)\(\s*['"]([A-Za-z0-9_.-]+)['"]\s*[),]/g,
  /\b(?:label|title|name|description|failedTip|placeholder)I18nKey\s*:\s*['"]([A-Za-z0-9_.-]+)['"]/g
]
const checkedKeys = new Set()
const missingUsedKeys = new Map()

for (const file of collectFiles(srcDir)) {
  const lines = fs.readFileSync(file, 'utf8').split('\n')
  const relPath = path.relative(process.cwd(), file)
  for (let i = 0; i < lines.length; i++) {
    for (const pattern of keyPatterns) {
      pattern.lastIndex = 0
      let match
      while ((match = pattern.exec(lines[i])) !== null) {
        const key = match[1]
        checkedKeys.add(key)
        for (const [locale, keys] of [['zh-CN', zhKeys], ['en-US', enKeys]]) {
          const id = `${key}|${locale}`
          if (!keys.has(key) && !missingUsedKeys.has(id)) {
            missingUsedKeys.set(id, { key, locale, file: relPath, line: i + 1 })
          }
        }
      }
    }
  }
}

if (missingUsedKeys.size) {
  console.error(`Missing i18n keys (${missingUsedKeys.size} issue(s)):`)
  for (const error of missingUsedKeys.values()) {
    console.error(`  [${error.locale}] ${error.key}  (${error.file}:${error.line})`)
  }
  process.exit(1)
}

console.log(`I18N locale keys OK (${enKeys.size} keys).`)
console.log(`I18N used keys OK (${checkedKeys.size} keys checked).`)
