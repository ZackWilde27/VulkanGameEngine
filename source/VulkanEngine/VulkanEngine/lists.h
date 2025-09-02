typedef struct _LinkedItem
{
	void* item;
	void* next;
} LinkedItem;

LinkedItem* GetLastItem(LinkedItem* list)
{
	while (list->next)
		list = list->next;

	return list;
}

void* Linked_GetIndex(LinkedItem* list, int index)
{
	while (index)
	{
		list = list->next;
		index--;
	}
	return list->item;
}

void AddLinkedItem(LinkedItem** list, void* data, bool unique)
{
	LinkedItem* newItem = malloc(sizeof(LinkedItem));
	if (!newItem)
	{
		printf("Out of Memory! Can\'t AddLinkedMesh()\n");
		return;
	}

	newItem->item = data;
	newItem->next = NULL;

	if (!(*list))
	{
		*list = newItem;
		return;
	}

	if (unique)
	{
		for (LinkedItem* i = *list; i; i = i->next)
		{
			if (data == i->item)
				return;
		}
	}

	GetLastItem(*list)->next = newItem;
}

void FreeThosePointers(void** tofree, unsigned short count)
{
	for (int i = 0; i < count; i++)
	{
		if (tofree[i])
		{
			free(tofree[i]);
		}
	}
}

void ClearLinkedList(LinkedItem* root)
{
	void* tofree[256];
	unsigned short freedex = 0;

	if (root)
	{
		while (root->next)
		{
			tofree[freedex++] = root;
			tofree[freedex++] = root->item;
			root = root->next;

			if (freedex >= 256)
			{
				FreeThosePointers(tofree, freedex);
                freedex -= 256;
			}
		}
		FreeThosePointers(tofree, freedex);
	}
}