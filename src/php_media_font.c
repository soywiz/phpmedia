void glyph_free(FontGlyphCache *g) {
	if (g == NULL) return;
	if (g->surface != NULL) {
		if (g->surface->refcount <= 1) {
			glDeleteTextures(1, &g->gltex);
			if (g->list) glDeleteLists(g->list, 1);
		}
		SDL_FreeSurface(g->surface);
	}
	g->ch = 0;
	g->used = 0;
	g->gltex = 0;
	g->list = 0;
	g->surface = NULL;
}

PM_OBJECTDELETE(Font)
{
	int n;

	if (object->font) {
		for (n = 0; n < GLYPH_MAX_CACHE; n++) glyph_free(&object->glyphs[n]);
		TTF_CloseFont(object->font);
	}

	PM_OBJECTDELETE_STD;
}

PM_OBJECTCLONE_IMPL(Font) {
	CLONE_COPY_FIELD(font);
}

PM_OBJECTNEW(Font);
PM_OBJECTCLONE(Font);

void FontCheckInit() { if (!TTF_WasInit()) TTF_Init(); }

char temp[0x800];

// Font::fromFile($file, $size = 16, $index = 0)
PHP_METHOD_ARGS(Font, fromFile) ARG_INFO(file) ARG_INFO(size) ARG_INFO(index) ZEND_END_ARG_INFO()
PHP_METHOD(Font, fromFile)
{
	char *name = NULL; int name_len = 0; int size = 16, index = 0;
	SDL_RWops *rw = NULL;
	FontStruct *object = NULL;
	
	sdl_load(TSRMLS_C);

	FontCheckInit(); if (zend_parse_parameters(ZEND_NUM_ARGS(), TSRMLS_C, "s|ll", &name, &name_len, &size, &index) == FAILURE) RETURN_FALSE;

	ObjectInit(EG(called_scope), return_value, TSRMLS_C); // Late Static Binding
	object = zend_object_store_get_object(return_value, TSRMLS_C);	
	object->font = NULL;
	
	rw = SDL_RWFromFile(name, "r");
	
#ifdef WIN32
	if (rw == NULL) {
		temp[0] = 0;
		SHGetSpecialFolderPath(NULL, temp, CSIDL_FONTS, 0);
		strcat(temp, "\\");
		strcat(temp, name);
		rw = SDL_RWFromFile(temp, "r");
	}
#endif
	
	if (rw != NULL) object->font = TTF_OpenFontIndexRW(rw, 1, size, index);

	if (object->font == NULL) {
		THROWF("Can't load font from file('%s') with size(%d)", name, size);
	}
}

SDL_RWops *TryOpenFontTTF(char *name) {
	HKEY key;
	LONG type;
	char temp[0x200], ttf_name[128];
	int len = sizeof(ttf_name);
	temp[0] = ttf_name[0] = 0;
	RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts", 0, KEY_ENUMERATE_SUB_KEYS | KEY_READ, &key);
	sprintf(temp, "%s (TrueType)", name);
	RegQueryValueEx(key, temp, 0, &type, ttf_name, &len);

	temp[0] = 0;
	SHGetSpecialFolderPath(NULL, temp, CSIDL_FONTS, 0);
	strcat(temp, "\\");
	strcat(temp, ttf_name);
	return SDL_RWFromFile(temp, "r");
}

// Font::fromName($name, $size = 16, $index = 0)
PHP_METHOD_ARGS(Font, fromName) ARG_INFO(name) ARG_INFO(size) ARG_INFO(index) ZEND_END_ARG_INFO()
PHP_METHOD(Font, fromName)
{
	char temp[0x200]; char *temp_start, *temp_end;
	char *name = NULL; int name_len = 0; int size = 16, index = 0;
	SDL_RWops *rw = NULL;
	FontStruct *object = NULL;
	
	sdl_load(TSRMLS_C);

	FontCheckInit(); if (zend_parse_parameters(ZEND_NUM_ARGS(), TSRMLS_C, "s|ll", &name, &name_len, &size, &index) == FAILURE) RETURN_FALSE;

	ObjectInit(EG(called_scope), return_value, TSRMLS_C); // Late Static Binding
	object = zend_object_store_get_object(return_value, TSRMLS_C);	
	object->font = NULL;
	
	sprintf(temp, "%s", name);
	temp_start = temp;
	
#ifdef WIN32
	while (1) {
		temp_end = strchr(temp_start, ',');
		if (temp_end) {
			int n;
			temp_end[0] = '\0';
			for (n = -1; isspace(temp_end[n]); n--) temp_end[n] = '\0';
		}
		while (isspace(*temp_start)) temp_start++;
		
		// Found!
		if ((rw = TryOpenFontTTF(temp_start)) != NULL) break;

		if (temp_end == NULL) break;
		temp_start = temp_end + 1;
	}
#else
	THROWF("Only implemented in windows.")
#endif
	
	if (rw != NULL) object->font = TTF_OpenFontIndexRW(rw, 1, size, index);

	if (object->font == NULL) {
		THROWF("Can't load font '%s' with size(%d)", name, size);
	}
}

// Font::fromString($data, $size = 16, $index = 0)
PHP_METHOD_ARGS(Font, fromString) ARG_INFO(data) ARG_INFO(size) ARG_INFO(index) ZEND_END_ARG_INFO()
PHP_METHOD(Font, fromString)
{
	sdl_load(TSRMLS_C);
}

// Font::width($str)
PHP_METHOD_ARGS(Font, width) ARG_INFO(str) ZEND_END_ARG_INFO()
PHP_METHOD(Font, width)
{
	Uint16 ch = 0;
	int minx, miny, maxx, maxy, advance, x, y, ptr_pos = 0, ptr_inc = 0, width = 0, max_width = 0;
	char *str = NULL; int str_len = 0;
	THIS_FONT;
	FontCheckInit(); if (zend_parse_parameters(ZEND_NUM_ARGS(), TSRMLS_C, "s", &str, &str_len) == FAILURE) RETURN_FALSE;

	while (ptr_pos < str_len) {
		ch = utf8_decode(&str[ptr_pos], &ptr_inc); ptr_pos += ptr_inc;
		if (ch == '\n') {
			if (width > max_width) max_width = width;
			width = 0;
			continue;
		}
		TTF_GlyphMetrics(font->font, ch, &minx, &maxx, &miny, &maxy, &advance);
		x = minx;
		y = TTF_FontAscent(font->font) - maxy;
		width += advance;
	}
	if (width > max_width) max_width = width;

	RETURN_LONG(max_width);
}

// Font::__get($key)
PHP_METHOD_ARGS(Font, __get) ARG_INFO(key) ZEND_END_ARG_INFO()
PHP_METHOD(Font, __get)
{
	char *key; int key_l;
	THIS_FONT;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), TSRMLS_C, "s", &key, &key_l) == FAILURE) RETURN_FALSE;

	if (strcmp(key, "fixed"    ) == 0) RETURN_BOOL(TTF_FontFaceIsFixedWidth(font->font));
	if (strcmp(key, "style"    ) == 0) RETURN_LONG(TTF_GetFontStyle(font->font));
	if (strcmp(key, "height"   ) == 0) RETURN_LONG(TTF_FontHeight(font->font));
	if (strcmp(key, "ascent"   ) == 0) RETURN_LONG(TTF_FontAscent(font->font));
	if (strcmp(key, "descent"  ) == 0) RETURN_LONG(TTF_FontDescent(font->font));
	if (strcmp(key, "lineSkip" ) == 0) RETURN_LONG(TTF_FontLineSkip(font->font));
	if (strcmp(key, "faceName" ) == 0) RETURN_STRING(TTF_FontFaceFamilyName(font->font), 1);
	if (strcmp(key, "styleName") == 0) RETURN_STRING(TTF_FontFaceStyleName(font->font), 1);
	
	RETURN_FALSE;
}

// Font::__set($key, $value)
PHP_METHOD_ARGS(Font, __set) ARG_INFO(key) ARG_INFO(value) ZEND_END_ARG_INFO()
PHP_METHOD(Font, __set)
{
	char *key; int key_l; int v;
	THIS_FONT;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), TSRMLS_C, "sl", &key, &key_l, &v) == FAILURE) RETURN_FALSE;

	if (strcmp(key, "style"   ) == 0) {
		TTF_SetFontStyle(font->font, v);
		RETURN_LONG(TTF_GetFontStyle(font->font));
	}
}

// Font::metrics($char)
PHP_METHOD_ARGS(Font, metrics) ARG_INFO(key) ZEND_END_ARG_INFO()
PHP_METHOD(Font, metrics)
{
	char *key; int key_l;
	zval *array;
	THIS_FONT;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), TSRMLS_C, "s", &key, &key_l) == FAILURE) RETURN_FALSE;
	{
		int minx, miny;
		int maxx, maxy;
		int advance;
		Uint16 ch = utf8_decode(key, NULL);
		TTF_GlyphMetrics(font->font, ch, &minx, &maxx, &miny, &maxy, &advance);
		
		MAKE_STD_ZVAL(array);
		array_init(array);

		add_assoc_long(array, "minx", minx);
		add_assoc_long(array, "miny", miny);
		add_assoc_long(array, "maxx", maxx);
		add_assoc_long(array, "maxy", maxy);
		add_assoc_long(array, "width", maxx - minx);
		add_assoc_long(array, "height", maxy - miny);
		add_assoc_long(array, "advance", advance);
		
		RETURN_ZVAL(array, 0, 1);
	}
}

// Font::height($text = '')
PHP_METHOD_ARGS(Font, height) ARG_INFO(text) ZEND_END_ARG_INFO()
PHP_METHOD(Font, height)
{
	char *text = NULL; int text_len = 0, lines = 1, n;
	THIS_FONT;
	FontCheckInit(); if (zend_parse_parameters(ZEND_NUM_ARGS(), TSRMLS_C, "|s", &text, &text_len) == FAILURE) RETURN_FALSE;
	for (n = 0; n < text_len; n++) {
		if (text[n] == '\n') lines++;
	}
	RETURN_LONG(TTF_FontHeight(font->font) * lines);
}

int sort_glyph_usage(const void *_a, const void *_b) {
	FontGlyphCache *a = (FontGlyphCache *)_a, *b = (FontGlyphCache *)_b;
	if (a->ch == 0) return +1;
	if (b->ch == 0) return -1;
	return (int)(b->used) - (int)(a->used);
}

int reduce_count = 0;

void glyph_reduce(FontStruct *font) {
	if (reduce_count++ <= 64000) return; else reduce_count = 0;
	{
		int n; FontGlyphCache *g;
		int min = 0xFFFF;

		for (n = 0; n < GLYPH_MAX_CACHE; n++) {
			g = &font->glyphs[n];
			//if (g->used == 0) continue;
			if (g->used == 0) break;
			if (g->used < min) min = g->used;
		}
		
		min--;

		//qsort(font->glyphs, GLYPH_MAX_CACHE, sizeof(FontGlyphCache), sort_glyph_usage);
		qsort(font->glyphs, n, sizeof(FontGlyphCache), sort_glyph_usage);

		//printf("{\n");
		for (n = 0; n < GLYPH_MAX_CACHE; n++) {
			g = &font->glyphs[n];
			//if (g->used == 0) continue;
			if (g->used == 0) break;
			g->used -= min;
			//printf("  %04X: %04X: %d\n", n, g->ch, g->used);
		}
		//printf("}");
	}
}

FontGlyphCache *glyph_get(FontStruct *font, Uint16 ch) {
	int n = 0;
	int w, h, x, y;
	SDL_Surface *surfaceogl = NULL;
	FontGlyphCache *g = NULL;
	SDL_Color color = {0xFF, 0xFF, 0xFF, 0xFF};

	glyph_reduce(font);

	// Try to locate an already cached glyph
	for (n = 0; n < GLYPH_MAX_CACHE; n++) {
		g = &font->glyphs[n];
		//if (g->ch != 0) printf("Check: (%d/%d)\n", ch, g->ch);
		if (g->ch == ch) {
			if (g->used < 0xFFFF - 1) g->used++;
			return g;
		}
		if (g->ch == 0) break;
	}
	//printf("Not cached (%d)!\n", ch);
	
	glyph_free(g);
	g->ch = ch;
	g->used = 1;
	g->surface = TTF_RenderGlyph_Blended(font->font, ch, color);
	g->list = glGenLists(1);
	surfaceogl = __SDL_ConvertSurfaceForOpenGL(g->surface, 0);
	{
		glGenTextures(1, &g->gltex);
		glBindTexture(GL_TEXTURE_2D, g->gltex);

		glTexImage2D(GL_TEXTURE_2D, 0, 4, surfaceogl->w, surfaceogl->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surfaceogl->pixels);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glEnable(GL_CLAMP_TO_EDGE);
		glEnable(GL_TEXTURE_2D);
	}

	w = surfaceogl->w;
	h = surfaceogl->h;
	x = 0;
	y = 0;

	SDL_FreeSurface(surfaceogl);

	glNewList(g->list, GL_COMPILE);
	{
		int minx, miny;
		int maxx, maxy;
		int advance;
		TTF_GlyphMetrics(font->font, ch, &minx, &maxx, &miny, &maxy, &advance);
		x = minx;
		y = TTF_FontAscent(font->font) - maxy;
		
		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();
		glScaled(1.0 / (double)w, 1.0 / (double)h, 1.0);

		glMatrixMode(GL_MODELVIEW);

		glBindTexture(GL_TEXTURE_2D, g->gltex);
		glEnable(GL_TEXTURE_2D);

		glBegin(GL_QUADS);
			if (1) {
				glTexCoord2f((float)0, (float)0); glVertex2i(x + 0, y + 0);
				glTexCoord2f((float)w, (float)0); glVertex2i(x + w, y + 0);
				glTexCoord2f((float)w, (float)h); glVertex2i(x + w, y + h);
				glTexCoord2f((float)0, (float)h); glVertex2i(x + 0, y + h);
			} else {
				glTexCoord2f(0, 0); glVertex2i(x + 0, y + 0);
				glTexCoord2f(1, 0); glVertex2i(x + w, y + 0);
				glTexCoord2f(1, 1); glVertex2i(x + w, y + h);
				glTexCoord2f(0, 1); glVertex2i(x + 0, y + h);
			}
		glEnd();
		glTranslatef((float)advance, 0.0, 0.0);
	}
	glEndList();
	
	return g;
}

// Font::blit(Bitmap $dest, $text, $x, $y, $color)
PHP_METHOD_ARGS(Font, blit) ARG_INFO(text) ZEND_END_ARG_INFO()
PHP_METHOD(Font, blit)
{
	zval *object_bitmap = NULL;
	zval *color_array = NULL;
	char *str = NULL; int str_len = 0;
	int ptr_pos = 0; int ptr_inc = 0;
	double x = 0.0, y = 0.0;
	double colorv[4] = {1, 1, 1, 1};
	FontGlyphCache *g;
	Uint16 ch = 0;
	int line = 0;
	THIS_FONT;
	FontCheckInit(); if (zend_parse_parameters(ZEND_NUM_ARGS(), TSRMLS_C, "Os|dda", &object_bitmap, ClassEntry_Bitmap, &str, &str_len, &x, &y, &color_array) == FAILURE) RETURN_FALSE;
	
	extract_color(color_array, colorv);

	glPushAttrib(GL_CURRENT_BIT);
	{
		glColor4dv(colorv);

		glLoadIdentity();
		glTranslated(x, y, 0.0);
		while (ptr_pos < str_len) {
			ch = utf8_decode(&str[ptr_pos], &ptr_inc); ptr_pos += ptr_inc;
			if (ch == '\n') {
				glLoadIdentity();
				glTranslated(x, y + ++line * TTF_FontHeight(font->font), 0.0);
				continue;
			}
			g = glyph_get(font, ch);
			glCallList(g->list);
		}
	}
	glPopAttrib();

	//glMatrixMode(GL_TEXTURE); glPopMatrix();
}
