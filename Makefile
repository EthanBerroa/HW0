all: help

NAME=hw0

help:
	@echo "This makefile is used to repack your $(NAME) code for submission."
	@echo "Type \"make repack groupid=YOUR_GROUPID\" to create a tarball."

repack: clean
ifdef groupid
	@echo "Packaging for $(NAME)..."
	@mkdir -p $(NAME)
	@cp -r Makefile ult $(NAME)/ 
	@tar zcf $(NAME)-$(groupid).tar.gz $(NAME)
	@rm -rf $(NAME)
	@echo "Created $(NAME)-$(groupid).tar.gz"
else
	@echo "You must specify a group id, please type \"make help\" for more details."
endif

clean:
	@echo "Cleaning for $(NAME)..."
	@make -C ult clean
	@rm -rf *.tar.gz $(NAME)
