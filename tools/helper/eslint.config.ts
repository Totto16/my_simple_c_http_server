import eslint from '@eslint/js'
import tseslint from 'typescript-eslint'
import { fileURLToPath } from 'node:url'
import { dirname } from 'node:path'

const __filename = fileURLToPath(import.meta.url)
const __dirname = dirname(__filename)

export default [
	{
		plugins: {
			tseslint: tseslint.plugin,
			eslint: eslint,
		},
	},
	{
		settings: {
			'import/resolver': {
				typescript: {
					project: [
						'tsconfig.json',
					],
				},
			},
		},
	},
	{
		ignores: [
			'build/**',
			"eslint.config.ts"
		],
	},
	eslint.configs.recommended,
	...tseslint.configs.strictTypeChecked,
	...tseslint.configs.stylisticTypeChecked,
	...tseslint.configs.recommendedTypeChecked,
	{
		languageOptions: {
			parserOptions: {
				project: './tsconfig.json',
				tsconfigRootDir: __dirname,
				warnOnUnsupportedTypeScriptVersion: true,
			},
		},
	},
	{
		rules: {
			'@typescript-eslint/no-unused-vars': [
				'error',
				{
					args: 'all',
					argsIgnorePattern: '^_',
					caughtErrors: 'all',
					caughtErrorsIgnorePattern: '^_',
					destructuredArrayIgnorePattern: '^_',
					varsIgnorePattern: '^_',
					ignoreRestSiblings: true,
				},
			],
			'@typescript-eslint/no-non-null-assertion': ['error'],
			curly: ['error', 'all'],
			'@typescript-eslint/explicit-function-return-type': 'error',
			'@typescript-eslint/consistent-type-exports': 'error',
			'@typescript-eslint/consistent-type-imports': 'error',
			"@typescript-eslint/prefer-nullish-coalescing": "off"
		},
	},
	{
		files: ['src/*.ts', 'index.ts'],
		languageOptions: {
			parserOptions: {
				project: './tsconfig.json',
				tsconfigRootDir: __dirname,
			},
		},
	},

]
